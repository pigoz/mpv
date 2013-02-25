/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "osdep/macosx_application_objc.h"
#include "video/out/vo.h"
#include "video/out/osx_common.h"
#include "core/input/input.h"
#include "core/mp_fifo.h"
#include "talloc.h"

// 0.0001 seems too much and 0.01 too low, no idea why this works so well
#define COCOA_MAGIC_TIMER_DELAY 0.001

static Application *app;

@interface Application (PrivateMethods)
- (NSMenuItem *)menuItemWithParent:(NSMenu *)parent
                             title:(NSString *)title
                            action:(SEL)selector
                     keyEquivalent:(NSString*)key;

- (NSMenuItem *)mainMenuItemWithParent:(NSMenu *)parent
                                 child:(NSMenu *)child;
- (void)registerMenuItem:(NSMenuItem*)menuItem forKey:(MPMenuKey)key;
- (NSMenu *)appleMenuWithMainMenu:(NSMenu *)mainMenu;
- (NSMenu *)movieMenu;
- (NSMenu *)windowMenu;
@end

@interface NSApplication (NiblessAdditions)
- (void)setAppleMenu:(NSMenu *)aMenu;
@end

@implementation Application
@synthesize files = _files;
@synthesize willStopOnOpenEvent = _will_stop_on_open_event;

- (id)init
{
    if (self = [super init]) {
        self->_menu_items = [[NSMutableDictionary alloc] init];
        self->_first_open_event_recived = NO;
        self->_will_stop_on_open_event = NO;
    }

    return self;
}

#define _R(P, T, E, K) \
    { \
        NSMenuItem *tmp = [self menuItemWithParent:(P) title:(T) \
                                            action:nil keyEquivalent:(E)]; \
        [self registerMenuItem:tmp forKey:(K)]; \
    }

- (NSMenu *)appleMenuWithMainMenu:(NSMenu *)mainMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Apple Menu"];
    [self mainMenuItemWithParent:mainMenu child:menu];
    [self menuItemWithParent:menu title:@"Quit mpv"
                      action:@selector(stop) keyEquivalent: @"q"];
    return [menu autorelease];
}

- (NSMenu *)movieMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Movie"];
    _R(menu, @"Half Size",   @"0", MPM_H_SIZE)
    _R(menu, @"Normal Size", @"1", MPM_N_SIZE)
    _R(menu, @"Double Size", @"2", MPM_D_SIZE)
    return [menu autorelease];
}

- (NSMenu *)windowMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Window"];
    _R(menu, @"Minimize", @"m", MPM_MINIMIZE)
    _R(menu, @"Zoom",     @"z", MPM_ZOOM)
    return [menu autorelease];
}

- (void)initialize_menu
{
    NSMenu *main_menu = [[NSMenu new] autorelease];
    [NSApp setMainMenu:main_menu];
    [NSApp setAppleMenu:[self appleMenuWithMainMenu:main_menu]];

    [app mainMenuItemWithParent:main_menu child:[self movieMenu]];
    [app mainMenuItemWithParent:main_menu child:[self windowMenu]];
}

#undef _R

- (void)setCallback:(play_loop_callback)callback
         andContext:(struct MPContext *)context
{
    self->_callback = callback;
    self->_context  = context;
}

- (void)call_callback
{
    if (self->_context->stop_play) {
        [self stop];
    } else {
        self->_callback(self->_context);
    }
}

- (void)schedule_timer
{
    self->_callback_timer =
        [NSTimer timerWithTimeInterval:COCOA_MAGIC_TIMER_DELAY
                                target:self
                              selector:@selector(call_callback)
                              userInfo:nil
                               repeats:YES];

    [[NSRunLoop currentRunLoop] addTimer:self->_callback_timer
                                forMode:NSDefaultRunLoopMode];

    [[NSRunLoop currentRunLoop] addTimer:self->_callback_timer
                                forMode:NSEventTrackingRunLoopMode];
}

- (void)stop
{
    [NSApp stop:nil];
    // Post a fake event so that the stop event inserted by cocoa is processed
    // I <3 cocoa bugs!
    cocoa_post_fake_event();
}

- (void)registerMenuItem:(NSMenuItem*)menuItem forKey:(MPMenuKey)key
{
    [self->_menu_items setObject:menuItem forKey:[NSNumber numberWithInt:key]];
}

- (void)registerSelector:(SEL)action forKey:(MPMenuKey)key
{
    NSNumber *boxedKey = [NSNumber numberWithInt:key];
    NSMenuItem *item   = [self->_menu_items objectForKey:boxedKey];
    if (item) {
        [item setAction:action];
    }
}

- (NSMenuItem *)menuItemWithParent:(NSMenu *)parent
                             title:(NSString *)title
                            action:(SEL)action
                     keyEquivalent:(NSString*)key
{

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:action
                                           keyEquivalent:key];
    [parent addItem:item];
    return [item autorelease];
}

- (NSMenuItem *)mainMenuItemWithParent:(NSMenu *)parent
                                 child:(NSMenu *)child
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@""
                                                  action:nil
                                           keyEquivalent:@""];
    [item setSubmenu:child];
    [parent addItem:item];
    return [item autorelease];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    self->_files = [filenames sortedArrayUsingSelector:@selector(compare:)];
    if (self->_first_open_event_recived) {
        for (int i = 0; i < [self->_files count]; i++) {
            NSString *filename = [self->_files objectAtIndex:i];
            NSString *escaped_filename = escape_loadfile_name(filename);
            char *cmd = talloc_asprintf(NULL, "loadfile \"%s\"%s",
                                        [escaped_filename UTF8String],
                                        (i == 0) ? "" : " append");
            mp_input_queue_cmd(self->_context->video_out->input_ctx,
                               mp_input_parse_cmd(bstr0(cmd), ""));
            talloc_free(cmd);
        }
    } else {
        self->_first_open_event_recived = YES;
        if (_will_stop_on_open_event)
            [NSApp stop:nil];
    }
}
@end

void cocoa_register_menu_item_action(MPMenuKey key, void* action)
{
    [app registerSelector:(SEL)action forKey:key];
}

void init_cocoa_application(void)
{
    NSApp = [NSApplication sharedApplication];
    app = [[Application alloc] init];
    [NSApp setDelegate:app];
    [app initialize_menu];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

void cocoa_run_runloop(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApp run];
    [pool drain];
}

void cocoa_run_loop_schedule(play_loop_callback callback,
                             struct MPContext *context)
{
    [NSApp setDelegate:app];
    [app setCallback:callback andContext:context];
    [app schedule_timer];
}

void cocoa_post_fake_event(void)
{
    NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0,0)
                                   modifierFlags:0
                                       timestamp:0.0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:true];
}
