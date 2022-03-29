package com.jhuster.audiodemo.tester;

import android.media.AudioFormat;
import android.media.MediaRecorder;
import android.os.Environment;

import com.jhuster.audiodemo.api.audio.AudioCapturer;
import com.jhuster.audiodemo.api.wav.WavFileWriter;

import java.io.IOException;

public class AudioCaptureTester extends Tester implements AudioCapturer.OnAudioFrameCapturedListener {

    private static final String DEFAULT_TEST_FILE = Environment.getExternalStorageDirectory() + "/test.wav";

    private AudioCapturer mAudioCapturer;
    private WavFileWriter mWavFileWriter;
    public static final int SAMPLE_RATE = 8000;

    @Override
    public boolean startTesting() {
        mAudioCapturer = new AudioCapturer();
        mWavFileWriter = new WavFileWriter();
        try {
            mWavFileWriter.openFile(DEFAULT_TEST_FILE, SAMPLE_RATE, 1, 16);
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
        mAudioCapturer.setOnAudioFrameCapturedListener(this);
        return mAudioCapturer.startCapture(MediaRecorder.AudioSource.MIC, SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
    }

    @Override
    public boolean stopTesting() {
        mAudioCapturer.stopCapture();
        try {
            mWavFileWriter.closeFile();
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    @Override
    public void onAudioFrameCaptured(byte[] audioData) {
        mWavFileWriter.writeData(audioData, 0, audioData.length);
    }
}
