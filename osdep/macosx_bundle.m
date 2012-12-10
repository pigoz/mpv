#include "osdep/macosx_bundle.h"

#import <Foundation/Foundation.h>

const char *get_bundled_path(const char *file)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSString *file2 =
      [[NSString alloc] initWithCString:file encoding:NSUTF8StringEncoding];
  [file2 autorelease];

  NSArray *components = [file2 componentsSeparatedByString:@"."];
  NSString *basename  = [components objectAtIndex:0];
  NSString *extension = [components objectAtIndex:1];

  NSString *path =
      [[NSBundle mainBundle] pathForResource:basename ofType:extension];
  const char *rv = [path UTF8String];
  [pool release];
  return rv;
}
