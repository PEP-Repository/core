// Source: https://sparkle-project.org/documentation/programmatic-setup/

// Compile SparkleUpdater.mm with -fobjc-arc because this file assumes Automatic Reference Counting (ARC) is enabled
#if  ! __has_feature(objc_arc)
    #error This file must be compiled with ARC. Use -fobjc-arc flag.
#endif

#include "SparkleUpdater.h"

// import means that we only include the file once
#import <Sparkle/Sparkle.h>
#import <SPUUpdater.h>

@interface AppUpdaterDelegate : NSObject <SPUUpdaterDelegate>

    @property (nonatomic) SPUStandardUpdaterController *updaterController;
    @property (nonatomic) SparkleUpdater *UpdaterInstance;

    // bridging function to call the C++ method changeUpdateStatus
    extern "C" void changeUpdateStatusC(SparkleUpdater *updaterInstance, bool status);

@end


// Standalone C function to call the C++ method changeUpdateStatus
extern "C" void changeUpdateStatusC(SparkleUpdater *updaterInstance, bool status) {
    updaterInstance->changeUpdateStatus(status);
}

@implementation AppUpdaterDelegate

 // Objective-c method syntax:
 // - (return_type) method_name: (argumentType1) argumentName1 joiningArgument2:( argumentType2 )argumentName2

    // override the method updater:didFindValidUpdate: to handle the update found event
    - (void) updater:(SPUUpdater *) updater didFindValidUpdate: (SUAppcastItem *) item {
        changeUpdateStatusC(self.UpdaterInstance, true);
    }

    - (void) updaterDidNotFindUpdate: (SPUUpdater *) updater {
        changeUpdateStatusC(self.UpdaterInstance, false);
    }

@end

// Constructor for SparkleUpdater class, a wrapper around AppUpdaterDelegate and SPUStandardUpdaterController objects.
SparkleUpdater::SparkleUpdater()
{
    @autoreleasepool {
        // Create AppUpdaterDelegate object and assign it to _updaterDelegate.
        // This is similar to creating an object with 'new'
        _updaterDelegate = [[AppUpdaterDelegate alloc] init];

        // Call the initWithStartingUpdater:updaterDelegate:userDriverDelegate: method on a 
        // newly allocated SPUStandardUpdaterController object and assign it to _updaterDelegate.updaterController
        // _updaterDelegate.updaterController is a SPUStandardUpdaterController
        _updaterDelegate.updaterController = [[SPUStandardUpdaterController alloc] initWithStartingUpdater: YES 
                                                                                   updaterDelegate: _updaterDelegate 
                                                                                   userDriverDelegate: nil];
        
        // Pass the SparkleUpdater instance to the AppUpdaterDelegate
        _updaterDelegate.UpdaterInstance = this;
    }
}

// Wrapper for the method checkForUpdates, a member function of UpdaterController.
void SparkleUpdater::checkForUpdates() {
    @autoreleasepool {
        // Call the checkForUpdates: method on _updaterDelegate.updaterController object with nullptr as the argument.
        [_updaterDelegate.updaterController checkForUpdates: nil];
    }
}

void SparkleUpdater::checkForUpdateInformation() {
    @autoreleasepool {
        //updater.automaticallyChecksForUpdates = YES;
        //updater.automaticallyDownloadsUpdates = YES;
        [_updaterDelegate.updaterController.updater checkForUpdateInformation];
    }
}

void SparkleUpdater::changeUpdateStatus(bool updateFound)
{
    emit updateStatusChanged(updateFound);
}