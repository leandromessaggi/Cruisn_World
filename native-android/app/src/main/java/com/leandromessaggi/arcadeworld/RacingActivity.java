package com.leandromessaggi.arcadeworld;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.content.pm.ActivityInfo;

/** Android-native Activity: no N64 ROM, emulation core, or WebView. */
public final class RacingActivity extends Activity {
    private RacingView game;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        game = new RacingView(this);
        setContentView(game);
        hideSystemUi();
    }

    private void hideSystemUi() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    @Override protected void onPause() {
        game.pauseGame();
        super.onPause();
    }
    @Override protected void onResume() {
        super.onResume();
        hideSystemUi();
        game.resumeGame();
    }
    @Override protected void onDestroy() {
        game.shutdown();
        super.onDestroy();
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (game != null && game.handleKeyEvent(event)) return true;
        return super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (game != null && game.handleMotion(event)) return true;
        return super.dispatchGenericMotionEvent(event);
    }
    @Override public void onBackPressed() {
        if (game != null) game.togglePause();
    }
}
