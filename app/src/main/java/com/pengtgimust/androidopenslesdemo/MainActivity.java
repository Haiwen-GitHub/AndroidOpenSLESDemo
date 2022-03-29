package com.pengtgimust.androidopenslesdemo;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.jhuster.audiodemo.tester.AudioCaptureTester;
import com.jhuster.audiodemo.tester.AudioCodecTester;
import com.jhuster.audiodemo.tester.AudioPlayerTester;
import com.jhuster.audiodemo.tester.NativeAudioTester;
import com.jhuster.audiodemo.tester.Tester;

public class MainActivity extends AppCompatActivity {
    private Spinner mTestSpinner;
    private Tester mTester;

    public static final String[] TEST_PROGRAM_ARRAY = {
            "录制 wav 文件",
            "播放 wav 文件",
            "OpenSL ES 录制",
            "OpenSL ES 播放",
            "音频编解码"
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String path = getExternalFilesDir(android.os.Environment.DIRECTORY_MUSIC).toString();
        Log.i("MainActivity onCreate", " "+path);
        mTestSpinner = (Spinner) findViewById(R.id.TestSpinner);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, TEST_PROGRAM_ARRAY);
        mTestSpinner.setAdapter(adapter);
    }

    public void onClickStartTest(View v) {
        switch (mTestSpinner.getSelectedItemPosition()) {
            case 0:
                mTester = new AudioCaptureTester();
                break;
            case 1:
                mTester = new AudioPlayerTester();
                break;
            case 2:
                mTester = new NativeAudioTester(true);
                break;
            case 3:
                mTester = new NativeAudioTester(false);
                break;
            case 4:
                mTester = new AudioCodecTester();
                break;
            default:
                break;
        }
        if (mTester != null) {
            mTester.startTesting();
            Toast.makeText(this, "Start Testing !", Toast.LENGTH_SHORT).show();
        }
    }

    public void onClickStopTest(View v) {
        if (mTester != null) {
            mTester.stopTesting();
            Toast.makeText(this, "Stop Testing !", Toast.LENGTH_SHORT).show();
        }
    }




//    // Used to load the 'native-lib' library on application startup.
//    static {
//        System.loadLibrary("native-lib");
//    }
//
//    @Override
//    protected void onCreate(Bundle savedInstanceState) {
//        super.onCreate(savedInstanceState);
//        setContentView(R.layout.activity_main);
//
//        // Example of a call to a native method
//        TextView tv = findViewById(R.id.sample_text);
//        tv.setText(stringFromJNI());
//
//        verifyStoragePermissions(this);
//        verifyInternetPermissions(this);
//    }
//
//    /**
//     * A native method that is implemented by the 'native-lib' native library,
//     * which is packaged with this application.
//     */
//    public native String stringFromJNI();
//
//
//    private static final int REQUEST_EXTERNAL_STORAGE = 1;
//    private static final int REQUEST_INTERNET = 0;
//
//    private static String[] PERMISSIONS_STORAGE = {
//            "android.permission.READ_EXTERNAL_STORAGE",
//            "android.permission.WRITE_EXTERNAL_STORAGE",
//            "android.permission.INTERNET"};
//    private static String[] PERMISSIONS_INTERNET = {
//            "android.permission.INTERNET"};
//
//    public static void verifyStoragePermissions(Activity activity) {
//        try {
//            //检测是否有写的权限
//            int permission = ActivityCompat.checkSelfPermission(activity,
//                    "android.permission.WRITE_EXTERNAL_STORAGE");
//            if (permission != PackageManager.PERMISSION_GRANTED) {
//                // 没有写的权限，去申请写的权限，会弹出对话框
//                ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE,REQUEST_EXTERNAL_STORAGE);
//            }
//        } catch (Exception e) {
//            e.printStackTrace();
//        }
//    }
//
//    public static void verifyInternetPermissions(Activity activity) {
//        try {
//            //检测是否有写的权限
//            int permission = ActivityCompat.checkSelfPermission(activity,
//                    "android.permission.INTERNET");
//            if (permission != PackageManager.PERMISSION_GRANTED) {
//                // 没有写的权限，去申请写的权限，会弹出对话框
//                ActivityCompat.requestPermissions(activity, PERMISSIONS_INTERNET,REQUEST_INTERNET);
//            }
//        } catch (Exception e) {
//            e.printStackTrace();
//        }
//    }


}
