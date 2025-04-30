// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_utils.h"

#include <UIKit/UIKit.h>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>

#include <AudioToolbox/AudioToolbox.h>
#include <WebKit/WKWebView.h>

void prepareWindow()
{
    if (QWindow *window = getMainWindow())
    {
        window->setFlags(window->flags() | Qt::MaximizeUsingFullscreenGeometryHint);
        window->showMaximized();
    }
    [UIApplication sharedApplication].statusBarStyle = UIStatusBarStyleLightContent;
}

void hideSystemUi() {
    [UIApplication sharedApplication].statusBarHidden = YES;
}

void showSystemUi() {
    [UIApplication sharedApplication].statusBarHidden = NO;
}

int statusBarHeight()
{
    const auto orientation = qApp->primaryScreen()->orientation();
    const bool isLandscape = orientation == Qt::LandscapeOrientation
        || orientation == Qt::InvertedLandscapeOrientation;

    // iOS phone does not have status bar in landscape mode
    if (isPhone() && isLandscape)
        return 0;

    CGSize size = [[UIApplication sharedApplication] statusBarFrame].size;
    return qMin(size.width, size.height);
}

void setScreenOrientation(Qt::ScreenOrientation orientation)
{
    const bool portrait = orientation == Qt::PrimaryOrientation
        || orientation == Qt::PortraitOrientation
        || orientation == Qt::InvertedPortraitOrientation;

    if (@available(iOS 16.0, *))
    {
        const int orientationValue = portrait
            ? UIInterfaceOrientationMaskPortrait
            : UIInterfaceOrientationMaskLandscapeRight;
        UIWindowScene * windowScene = (UIWindowScene *)
                                     [[[UIApplication sharedApplication] connectedScenes] allObjects].firstObject;
        UIWindowSceneGeometryPreferencesIOS* perference = [[ UIWindowSceneGeometryPreferencesIOS alloc]
            initWithInterfaceOrientations: orientationValue];
        [windowScene requestGeometryUpdateWithPreferences:perference errorHandler: nil];
    }
    else
    {
        const int orientationValue = portrait
            ? UIDeviceOrientationPortrait
            : UIDeviceOrientationLandscapeRight;
        NSNumber *value = [NSNumber numberWithInt: orientationValue];
        [[UIDevice currentDevice] setValue:value forKey:@"orientation"];
        [UIViewController attemptRotationToDeviceOrientation];
    }
}

bool isPhone() {
    return UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone;
}

void setKeepScreenOn(bool keepScreenOn)
{
    const auto application = [UIApplication sharedApplication];
    [application setIdleTimerDisabled:(keepScreenOn ? YES : NO)];
}

void makeShortVibration()
{
    static constexpr int kShortVibrationId = 1519;
    AudioServicesPlaySystemSound(kShortVibrationId);
}
