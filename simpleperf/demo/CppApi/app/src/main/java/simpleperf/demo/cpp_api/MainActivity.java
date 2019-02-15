package simpleperf.demo.cpp_api;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    TextView textView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        textView = findViewById(R.id.textView);
        runNativeCode();

        createUpdateViewThread();
    }

    void createUpdateViewThread() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {}
                    final long count = getBusyThreadCount();
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            textView.setText("Count: " + count);
                        }
                    });
                }
            }
        }).start();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    private native void runNativeCode();
    private native long getBusyThreadCount();

}
