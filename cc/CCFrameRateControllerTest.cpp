// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCFrameRateController.h"

#include "CCSchedulerTestCommon.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace WebKitTests;
using namespace WTF;

namespace {

class FakeCCFrameRateControllerClient : public cc::CCFrameRateControllerClient {
public:
    FakeCCFrameRateControllerClient() { reset(); }

    void reset() { m_vsyncTicked = false; }
    bool vsyncTicked() const { return m_vsyncTicked; }

    virtual void vsyncTick() { m_vsyncTicked = true; }

protected:
    bool m_vsyncTicked;
};


TEST(CCFrameRateControllerTest, TestFrameThrottling_ImmediateAck)
{
    FakeCCThread thread;
    FakeCCFrameRateControllerClient client;
    base::TimeDelta interval = base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
    RefPtr<FakeCCDelayBasedTimeSource> timeSource = FakeCCDelayBasedTimeSource::create(interval, &thread);
    CCFrameRateController controller(timeSource);

    controller.setClient(&client);
    controller.setActive(true);

    base::TimeTicks elapsed; // Muck around with time a bit

    // Trigger one frame, make sure the vsync callback is called
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew
    controller.didBeginFrame();

    // Tell the controller the frame ended 5ms later
    timeSource->setNow(timeSource->now() + base::TimeDelta::FromMilliseconds(5));
    controller.didFinishFrame();

    // Trigger another frame, make sure vsync runs again
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

TEST(CCFrameRateControllerTest, TestFrameThrottling_TwoFramesInFlight)
{
    FakeCCThread thread;
    FakeCCFrameRateControllerClient client;
    base::TimeDelta interval = base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
    RefPtr<FakeCCDelayBasedTimeSource> timeSource = FakeCCDelayBasedTimeSource::create(interval, &thread);
    CCFrameRateController controller(timeSource);

    controller.setClient(&client);
    controller.setActive(true);
    controller.setMaxFramesPending(2);

    base::TimeTicks elapsed; // Muck around with time a bit

    // Trigger one frame, make sure the vsync callback is called
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew
    controller.didBeginFrame();

    // Trigger another frame, make sure vsync callback runs again
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Tell the controller we drew, again.
    controller.didBeginFrame();

    // Trigger another frame. Since two frames are pending, we should not draw.
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_FALSE(client.vsyncTicked());

    // Tell the controller the first frame ended 5ms later
    timeSource->setNow(timeSource->now() + base::TimeDelta::FromMilliseconds(5));
    controller.didFinishFrame();

    // Tick should not have been called
    EXPECT_FALSE(client.vsyncTicked());

    // Trigger yet another frame. Since one frames is pending, another vsync callback should run.
    elapsed += base::TimeDelta::FromMilliseconds(thread.pendingDelayMs());
    EXPECT_TRUE(elapsed >= timeSource->now()); // Sanity check that previous code didn't move time backward.
    timeSource->setNow(elapsed);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

TEST(CCFrameRateControllerTest, TestFrameThrottling_Unthrottled)
{
    FakeCCThread thread;
    FakeCCFrameRateControllerClient client;
    CCFrameRateController controller(&thread);

    controller.setClient(&client);
    controller.setMaxFramesPending(2);

    // setActive triggers 1st frame, make sure the vsync callback is called
    controller.setActive(true);
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // Even if we don't call didBeginFrame, CCFrameRateController should
    // still attempt to vsync tick multiple times until it does result in
    // a didBeginFrame.
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // didBeginFrame triggers 2nd frame, make sure the vsync callback is called
    controller.didBeginFrame();
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
    client.reset();

    // didBeginFrame triggers 3rd frame (> maxFramesPending), make sure the vsync callback is NOT called
    controller.didBeginFrame();
    thread.runPendingTask();
    EXPECT_FALSE(client.vsyncTicked());
    client.reset();

    // Make sure there is no pending task since we can't do anything until we receive a didFinishFrame anyway.
    EXPECT_FALSE(thread.hasPendingTask());

    // didFinishFrame triggers a frame, make sure the vsync callback is called
    controller.didFinishFrame();
    thread.runPendingTask();
    EXPECT_TRUE(client.vsyncTicked());
}

}
