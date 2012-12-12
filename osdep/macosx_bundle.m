#include "osdep/macosx_bundle.h"

#include "talloc.h"
#import <Foundation/Foundation.h>

char *get_bundled_path(const char *file)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *path = [[NSBundle mainBundle] resourcePath];
  path = [path stringByAppendingFormat:@"/%s", file];
  char *rv = talloc_strdup(NULL, [path UTF8String]);
  [pool release];
  return rv;
}
