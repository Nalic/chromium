// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.media.AudioManager;
import android.os.Vibrator;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

/**
 * This is the implementation of the C++ counterpart VibrationProvider.
 */
@JNINamespace("content")
class VibrationProvider {

    private final AudioManager mAudioManager;
    private final Vibrator mVibrator;

    @CalledByNative
    private static VibrationProvider create(Context context) {
        return new VibrationProvider(context);
    }

    @CalledByNative
    private void vibrate(long milliseconds) {
        if (mAudioManager.getRingerMode() != AudioManager.RINGER_MODE_SILENT)
            mVibrator.vibrate(milliseconds);
    }

    @CalledByNative
    private void cancelVibration() {
        mVibrator.cancel();
    }

    private VibrationProvider(Context context) {
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        mVibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
    }
}
