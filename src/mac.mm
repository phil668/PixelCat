#import <Cocoa/Cocoa.h>
#import <ImageIO/ImageIO.h>
#include <mach/mach.h>
#include "motion.h"

@interface CatView : NSView { @public Behavior behavior; }
@property CGImageRef sprite;
@property NSMutableArray *frames;
@property id specialImage;
@property double lastTick;
@property BOOL held;
- (void)interact;
- (BOOL)playSpecialIndex:(int)index;
@property BOOL paused;
@property double tapped;
@property NSPoint anchor;
@property NSPoint startOrigin;
@property BOOL dragged;
@property NSTimer *timer;
- (void)startTimer;
@end

@implementation CatView
- (void)dealloc { if (_sprite) CGImageRelease(_sprite); }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }
- (BOOL)isOpaque { return NO; }
- (void)startTimer {
    [self.timer invalidate]; self.timer = nil;
    self.lastTick = NSProcessInfo.processInfo.systemUptime;
    if (!self.paused && self.window.visible) {
        self.timer = [NSTimer timerWithTimeInterval:(behavior.sleeping ? 2.0 : 0.125) target:self selector:@selector(tick:) userInfo:nil repeats:YES];
        self.timer.tolerance = 0.025;
        [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSRunLoopCommonModes];
    }
    self.needsDisplay = YES;
}
- (void)interact { behavior.interact(); [self startTimer]; }
- (BOOL)playSpecialIndex:(int)index {
    NSArray *names = @[@"groom",@"flower",@"belly",@"downcast",@"stretch",@"doze"];
    if (index<0 || index>=SpecialCount) return NO;
    NSURL *url = [NSBundle.mainBundle URLForResource:names[index] withExtension:@"png"];
    CGImageSourceRef source = url ? CGImageSourceCreateWithURL((__bridge CFURLRef)url,NULL) : NULL;
    if (!source) return NO;
    NSDictionary *opts = @{(id)kCGImageSourceCreateThumbnailFromImageAlways:@YES,(id)kCGImageSourceThumbnailMaxPixelSize:@512,(id)kCGImageSourceShouldCacheImmediately:@YES};
    CGImageRef img = CGImageSourceCreateThumbnailAtIndex(source,0,(__bridge CFDictionaryRef)opts); CFRelease(source);
    if (!img) return NO;
    self.specialImage = CFBridgingRelease(img);
    behavior.playSpecial(index);
    [self startTimer];
    return YES;
}
- (void)tick:(NSTimer *)timer {
    double now = NSProcessInfo.processInfo.systemUptime, dt = now-self.lastTick; self.lastTick = now;
    NSRect frame = self.window.frame, work = (self.window.screen ?: NSScreen.mainScreen).visibleFrame;
    double x = behavior.advance(dt,frame.origin.x,NSMinX(work),NSMaxX(work),frame.size.width,self.paused || self.held);
    if (x != frame.origin.x) [self.window setFrameOrigin:NSMakePoint(x,frame.origin.y)];
    if (self.window.visible) self.needsDisplay = YES;
    if (timer.timeInterval != (behavior.sleeping ? 2.0 : 0.125)) [self startTimer];
}
- (void)drawRect:(NSRect)dirty {
    [[NSColor clearColor] set]; NSRectFillUsingOperation(self.bounds, NSCompositingOperationCopy);
    Pose p = pose(NSProcessInfo.processInfo.systemUptime, self.tapped, self.paused);
    CGContextRef c = NSGraphicsContext.currentContext.CGContext;
    CGContextSetInterpolationQuality(c, kCGInterpolationNone);
    CGImageRef img = self.sprite;
    BOOL special = behavior.special >= 0 && self.specialImage != nil;
    if (!special) self.specialImage = nil;
    BOOL action = !special && (behavior.activity() == Activity::Walk || behavior.activity() == Activity::Sleep);
    if (special) img = (__bridge CGImageRef)self.specialImage;
    else if (action) img = (__bridge CGImageRef)self.frames[behavior.frame()];
    double maxW = self.bounds.size.width, maxH = self.bounds.size.height-22;
    double factor = std::min(maxW/CGImageGetWidth(img),maxH/CGImageGetHeight(img));
    double w = CGImageGetWidth(img)*factor, h = CGImageGetHeight(img)*factor;
    if (action && !behavior.sleeping && behavior.direction < 0) {
        CGContextTranslateCTM(c,self.bounds.size.width,0); CGContextScaleCTM(c,-1,1);
    }
    double sway = special ? 1.5*std::sin(behavior.clock*2.5) : 0;
    double stretch = special ? 1+0.005*std::sin(behavior.clock*1.8) : (action?1:p.scaleY);
    CGContextDrawImage(c, CGRectMake((self.bounds.size.width-w)/2+sway, 3+((action||special)?0:p.rise), w, h*stretch), img);
}
- (void)mouseDown:(NSEvent *)event {
    self.held = YES;
    self.anchor = NSEvent.mouseLocation; self.startOrigin = self.window.frame.origin; self.dragged = NO;
}
- (void)mouseDragged:(NSEvent *)event {
    NSPoint now = NSEvent.mouseLocation;
    if (!self.dragged && hypot(now.x-self.anchor.x, now.y-self.anchor.y) > 3) { self.dragged = YES; [self interact]; }
    if (self.dragged) [self.window setFrameOrigin:NSMakePoint(self.startOrigin.x+now.x-self.anchor.x, self.startOrigin.y+now.y-self.anchor.y)];
}
- (void)mouseUp:(NSEvent *)event {
    self.held = NO;
    if (!self.dragged) [self playSpecialIndex:behavior.randomSpecial(arc4random_uniform(SpecialCount))];
    NSRect r = self.window.frame, bounds = (self.window.screen ?: NSScreen.mainScreen).visibleFrame;
    [self.window setFrameOrigin:NSMakePoint(clampOrigin(r.origin.x,bounds.origin.x,NSMaxX(bounds),r.size.width),clampOrigin(r.origin.y,bounds.origin.y,NSMaxY(bounds),r.size.height))];
    [NSUserDefaults.standardUserDefaults setObject:NSStringFromPoint(self.window.frame.origin) forKey:@"origin"];
}
- (void)rightMouseDown:(NSEvent *)event { [NSMenu popUpContextMenu:NSApp.mainMenu withEvent:event forView:self]; }
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property NSPanel *panel;
@property CatView *cat;
@property NSStatusItem *status;
@property NSMenuItem *pauseItem;
@property NSMenu *sizeMenu;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)note {
    self.panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0,0,192,264) styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel backing:NSBackingStoreBuffered defer:NO];
    self.panel.opaque = NO; self.panel.backgroundColor = NSColor.clearColor; self.panel.hasShadow = NO;
    self.panel.level = NSFloatingWindowLevel; self.panel.hidesOnDeactivate = NO;
    self.panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
    self.cat = [[CatView alloc] initWithFrame:NSMakeRect(0,0,192,264)]; self.cat.tapped = -100;
    NSURL *url = [NSBundle.mainBundle URLForResource:@"cat" withExtension:@"png"];
    CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!source) { NSLog(@"Missing cat.png"); [NSApp terminate:nil]; return; }
    NSDictionary *opts = @{(id)kCGImageSourceCreateThumbnailFromImageAlways:@YES,(id)kCGImageSourceThumbnailMaxPixelSize:@512,(id)kCGImageSourceShouldCacheImmediately:@YES};
    self.cat.sprite = CGImageSourceCreateThumbnailAtIndex(source,0,(__bridge CFDictionaryRef)opts); CFRelease(source);
    if (!self.cat.sprite) { [NSApp terminate:nil]; return; }
    self.cat.frames = [NSMutableArray array];
    NSURL *actionsURL = [NSBundle.mainBundle URLForResource:@"actions" withExtension:@"png"];
    CGImageSourceRef actionSource = actionsURL ? CGImageSourceCreateWithURL((__bridge CFURLRef)actionsURL,NULL) : NULL;
    if (actionSource) {
        CGImageRef atlas = CGImageSourceCreateThumbnailAtIndex(actionSource,0,(__bridge CFDictionaryRef)opts);
        CFRelease(actionSource);
        if (atlas) {
            size_t w=CGImageGetWidth(atlas)/2,h=CGImageGetHeight(atlas)/2;
            for (int i=0;i<4;++i) {
                CGImageRef frame = CGImageCreateWithImageInRect(atlas,CGRectMake((i%2)*w,(i/2)*h,w,h));
                if (frame) [self.cat.frames addObject:CFBridgingRelease(frame)];
            }
            CGImageRelease(atlas);
        }
    }
    if (self.cat.frames.count != 4) { NSLog(@"Missing action frames"); [NSApp terminate:nil]; return; }
    NSUserDefaults *prefs = NSUserDefaults.standardUserDefaults;
    [prefs registerDefaults:@{@"roaming":@YES,@"autoSleep":@YES}];
    self.cat->behavior.roaming = [prefs boolForKey:@"roaming"];
    self.cat->behavior.autoSleep = [prefs boolForKey:@"autoSleep"];
    self.panel.contentView = self.cat;
    NSMenu *menu = [[NSMenu alloc] init];
    NSMenuItem *title = [menu addItemWithTitle:@"毛毛" action:nil keyEquivalent:@""]; title.enabled = NO;
    [menu addItem:NSMenuItem.separatorItem];
    self.pauseItem = [menu addItemWithTitle:@"暂停动画" action:@selector(togglePause:) keyEquivalent:@""];
    [menu addItemWithTitle:@"自动走动" action:@selector(toggleRoaming:) keyEquivalent:@""];
    [menu addItemWithTitle:@"两分钟后自动睡觉" action:@selector(toggleAutoSleep:) keyEquivalent:@""];
    [menu addItemWithTitle:@"睡觉 / 唤醒" action:@selector(toggleNap:) keyEquivalent:@""];
    NSMenuItem *poses = [menu addItemWithTitle:@"动作" action:nil keyEquivalent:@""];
    poses.submenu = [[NSMenu alloc] initWithTitle:@"动作"];
    NSArray *poseNames = @[@"鸡腿",@"花花",@"翻肚皮",@"耍帅",@"趴着卖萌",@"蜷身小憩"];
    for (NSInteger i=0;i<SpecialCount;++i) {
        NSMenuItem *item = [poses.submenu addItemWithTitle:poseNames[i] action:@selector(playSpecial:) keyEquivalent:@""];
        item.tag=i; item.target=self;
    }
    [menu addItemWithTitle:@"显示 / 隐藏" action:@selector(toggleVisible:) keyEquivalent:@""];
    NSMenuItem *sizeItem = [menu addItemWithTitle:@"调整大小" action:nil keyEquivalent:@""];
    self.sizeMenu = [[NSMenu alloc] initWithTitle:@"调整大小"];
    sizeItem.submenu = self.sizeMenu;
    for (NSNumber *percent in @[@50,@75,@100,@125,@150,@200]) {
        NSMenuItem *item = [self.sizeMenu addItemWithTitle:[NSString stringWithFormat:@"%@%%%@",percent,percent.intValue==100?@"（默认）":@""] action:@selector(changeSize:) keyEquivalent:@""];
        item.tag = percent.integerValue; item.target = self;
    }
    NSInteger savedSize = [NSUserDefaults.standardUserDefaults integerForKey:@"sizePercent"];
    NSMenuItem *selected = [self.sizeMenu itemWithTag:savedSize] ?: [self.sizeMenu itemWithTag:100];
    [self changeSize:selected];
    [menu addItemWithTitle:@"恢复位置" action:@selector(resetPosition:) keyEquivalent:@""];
    [menu addItem:NSMenuItem.separatorItem];
    [menu addItemWithTitle:@"退出毛毛" action:@selector(quit:) keyEquivalent:@""];
    for (NSMenuItem *item in menu.itemArray) item.target = self;
    NSApp.mainMenu = menu;
    self.status = [NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    self.status.button.title = @"🐾"; self.status.menu = menu;
    [self resetPosition:nil];
    NSString *saved = [NSUserDefaults.standardUserDefaults stringForKey:@"origin"];
    if (saved) {
        NSRect candidate = NSMakeRect(NSPointFromString(saved).x,NSPointFromString(saved).y,self.panel.frame.size.width,self.panel.frame.size.height);
        for (NSScreen *screen in NSScreen.screens) if (NSContainsRect(screen.visibleFrame,candidate)) { [self.panel setFrameOrigin:candidate.origin]; break; }
    }
    [self.panel orderFrontRegardless]; [self.cat startTimer];
    [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(writeMetrics:) userInfo:nil repeats:NO];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(sleep:) name:NSWorkspaceWillSleepNotification object:nil];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(wake:) name:NSWorkspaceDidWakeNotification object:nil];
}
- (void)writeMetrics:(NSTimer *)timer {
    task_vm_info_data_t info{}; mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) return;
    NSDictionary *report = @{@"physicalFootprintMiB":@(info.phys_footprint/1048576.0),@"residentMiB":@(info.resident_size/1048576.0),@"sampleSecondsAfterLaunch":@10,@"paused":@(self.cat.paused),@"spriteWidth":@(CGImageGetWidth(self.cat.sprite)),@"spriteHeight":@(CGImageGetHeight(self.cat.sprite))};
    NSURL *out = [[NSBundle.mainBundle.bundleURL URLByDeletingLastPathComponent] URLByAppendingPathComponent:@"PixelCat-metrics.json"];
    [[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil] writeToURL:out atomically:YES];
}
- (void)playSpecial:(NSMenuItem *)sender {
    if (![self.cat playSpecialIndex:(int)sender.tag]) NSBeep();
}
- (BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(toggleRoaming:)) item.state = self.cat->behavior.roaming ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(toggleAutoSleep:)) item.state = self.cat->behavior.autoSleep ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(toggleNap:)) item.title = self.cat->behavior.sleeping ? @"唤醒毛毛" : @"让毛毛睡觉";
    return item.action != nil || item.submenu != nil;
}
- (void)toggleRoaming:(id)sender {
    self.cat->behavior.roaming = !self.cat->behavior.roaming;
    [NSUserDefaults.standardUserDefaults setBool:self.cat->behavior.roaming forKey:@"roaming"];
    [self.cat interact];
}
- (void)toggleAutoSleep:(id)sender {
    self.cat->behavior.autoSleep = !self.cat->behavior.autoSleep;
    [NSUserDefaults.standardUserDefaults setBool:self.cat->behavior.autoSleep forKey:@"autoSleep"];
}
- (void)toggleNap:(id)sender {
    if (self.cat->behavior.sleeping) [self.cat interact]; else { self.cat->behavior.nap(); [self.cat startTimer]; }
}
- (void)sleep:(NSNotification *)n { [self.cat.timer invalidate]; }
- (void)wake:(NSNotification *)n { if (self.panel.visible) [self.cat startTimer]; }
- (void)togglePause:(id)sender { self.cat.paused = !self.cat.paused; self.pauseItem.title = self.cat.paused ? @"继续动画" : @"暂停动画"; [self.cat startTimer]; }
- (void)toggleVisible:(id)sender { if (self.panel.visible) { [self.panel orderOut:nil]; [self.cat.timer invalidate]; } else { [self.panel orderFrontRegardless]; [self.cat startTimer]; } }
- (void)changeSize:(NSMenuItem *)sender {
    double scale = sender.tag / 100.0;
    NSRect frame = self.panel.frame;
    NSRect work = (self.panel.screen ?: NSScreen.mainScreen).visibleFrame;
    NSSize size = NSMakeSize(192*scale,264*scale);
    frame.origin.x = clampOrigin(NSMidX(frame)-size.width/2,NSMinX(work),NSMaxX(work),size.width);
    frame.origin.y = clampOrigin(frame.origin.y,NSMinY(work),NSMaxY(work),size.height);
    frame.size = size;
    [self.panel setFrame:frame display:YES];
    [self.cat setFrameSize:size];
    self.cat.needsDisplay = YES;
    for (NSMenuItem *item in self.sizeMenu.itemArray) item.state = item==sender ? NSControlStateValueOn : NSControlStateValueOff;
    [NSUserDefaults.standardUserDefaults setInteger:sender.tag forKey:@"sizePercent"];
}
- (void)resetPosition:(id)sender { NSRect r = NSScreen.mainScreen.visibleFrame; [self.panel setFrameOrigin:NSMakePoint(clampOrigin(NSMaxX(r)-self.panel.frame.size.width-20,NSMinX(r),NSMaxX(r),self.panel.frame.size.width),r.origin.y+10)]; }
- (void)quit:(id)sender { [NSApp terminate:nil]; }
@end
int main() { @autoreleasepool { NSApplication *app = NSApplication.sharedApplication; [app setActivationPolicy:NSApplicationActivationPolicyAccessory]; AppDelegate *delegate = [AppDelegate new]; app.delegate = delegate; [app run]; } }
