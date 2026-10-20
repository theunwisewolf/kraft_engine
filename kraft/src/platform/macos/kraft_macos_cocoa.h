#pragma once

// The system frameworks kraft's Mac side calls into. MacTypes.h declares a global Rect and Point, which would
// collide with kraft's own in any program that says `using namespace kraft`, so they get other names here.
#define Rect KraftMacTypesRect
#define Point KraftMacTypesPoint
#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#import <Network/Network.h>
#import <Security/Security.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#undef Rect
#undef Point
