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

#include "osdep/macosx_application.h"
#import "Cocoa/Cocoa.h"

// 0.0001 seems too much and 0.01 too low, no idea why this works so well
#define COCOA_MAGIC_TIMER_DELAY 0.001

@interface Application : NSObject<NSApplicationDelegate> {
    play_loop_callback _callback;
    struct MPContext*  _context;
    NSTimer*           _callback_timer;
}

- (void)setCallback:(play_loop_callback)callback
         andContext:(struct MPContext *)context;
- (void)call_callback;
- (void)schedule_timer;
- (void)stop;
@end

@implementation Application
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
@end

static Application *app; // better than a singleton

void init_cocoa_application(void)
{
    NSApp = [NSApplication sharedApplication];
    app = [[Application alloc] init];
    [NSApp setDelegate:app];
    [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];
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
    [app setCallback:callback andContext:context];
    [app schedule_timer];
}

void cocoa_post_fake_event(void)
{
    NSEvent* event = [NSEvent otherEventWithType: NSApplicationDefined
                                        location: NSMakePoint(0,0)
                                   modifierFlags: 0
                                       timestamp: 0.0
                                    windowNumber: 0
                                         context: nil
                                         subtype: 0
                                           data1: 0
                                           data2: 0];
    [NSApp postEvent: event atStart: true];
}
