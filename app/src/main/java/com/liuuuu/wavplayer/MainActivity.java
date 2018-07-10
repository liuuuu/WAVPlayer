package com.liuuuu.wavplayer;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import java.io.File;
import java.io.IOException;

public class MainActivity extends Activity implements View.OnClickListener {

    /**
     * 文件名编译文本。
     */
    private EditText fileNameEdit;

    /**
     * On Create
     *
     * @param savedInstanceState 保持当前状态
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        fileNameEdit = findViewById(R.id.fileNameEdit);
        Button playButton = findViewById(R.id.playButton);
        playButton.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.playButton:
                onPlayButtonClick();
                break;
        }

    }

    /**
     * 单击播放按钮
     */
    private void onPlayButtonClick() {
        // 位于外部存储器
        File file = new File(Environment.getExternalStorageDirectory(), fileNameEdit.getText().toString());

        // 开始播放
        PlayTask playTask = new PlayTask();
        playTask.execute(file.getAbsolutePath());
    }

    private class PlayTask extends AsyncTask<String, Void, Exception> {
        /**
         * 后台播放任务
         *
         * @param file
         * @return
         */
        @Override
        protected Exception doInBackground(String... file) {
            Exception result = null;
            try {
                // 播放 WAVE 文件
                play(file[0]);
            } catch (IOException ex) {
                result = ex;
            }
            return result;
        }

        @Override
        protected void onPostExecute(Exception ex) {
            // 如果播放失败则显示错误信息
            if (ex != null) {
                new AlertDialog.Builder(MainActivity.this)
                        .setTitle(R.string.error_alert_title)
                        .setMessage(ex.getMessage()).show();
            }
        }
    }

    /**
     * 使用原生API播放指定的 WAVE 文件
     *
     * @param fileName
     * @throws IOException
     */
    private native void play(String fileName) throws IOException;

    static {
        System.loadLibrary("WAVPlayer");
    }
}
