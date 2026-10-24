package zzmod;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

/**
 * Floating mod menu for the offline game. Started from main.Main.onCreate.
 * All state lives in libkmod.so; this class is only the UI.
 */
public final class ModMenu {
    private static native void nStart();
    private static native void nSetMult(int v);
    private static native void nSetNoFatigue(int on);
    private static native int nGetState();

    private static boolean inited = false;
    private static boolean nativeOk = false;

    private static Activity act;
    private static SharedPreferences prefs;
    private static float dp;
    private static FrameLayout.LayoutParams btnLp;
    private static FrameLayout.LayoutParams panelLp;
    private static TextView button;
    private static LinearLayout panel;
    private static TextView multLabel;
    private static TextView statusLabel;
    private static final Handler handler = new Handler(Looper.getMainLooper());

    private static final Runnable ticker = new Runnable() {
        public void run() {
            updateStatus();
            if (panel != null && panel.getVisibility() == View.VISIBLE) {
                handler.postDelayed(this, 1000);
            }
        }
    };

    public static void init(final Activity a) {
        if (inited) return;
        inited = true;
        act = a;
        try {
            prefs = a.getSharedPreferences("zzmod", Context.MODE_PRIVATE);
        } catch (Throwable t) {
            prefs = null;
        }
        try {
            System.loadLibrary("kmod");
            nativeOk = true;
            nStart();
            nSetMult(getInt("mult", 1));
            nSetNoFatigue(getInt("nofat", 0));
        } catch (Throwable t) {
            nativeOk = false;
        }
        a.getWindow().getDecorView().post(new Runnable() {
            public void run() {
                try {
                    build();
                } catch (Throwable t) {
                    // never break the game because of the overlay
                }
            }
        });
    }

    private static int getInt(String k, int d) {
        try {
            return prefs != null ? prefs.getInt(k, d) : d;
        } catch (Throwable t) {
            return d;
        }
    }

    private static void putInt(String k, int v) {
        try {
            if (prefs != null) prefs.edit().putInt(k, v).apply();
        } catch (Throwable t) {
        }
    }

    private static int px(float v) {
        return (int) (v * dp + 0.5f);
    }

    private static GradientDrawable rounded(int color, float radiusDp) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(color);
        g.setCornerRadius(px(radiusDp));
        return g;
    }

    private static void screenSize(int[] out) {
        DisplayMetrics m = new DisplayMetrics();
        act.getWindowManager().getDefaultDisplay().getMetrics(m);
        out[0] = m.widthPixels;
        out[1] = m.heightPixels;
    }

    private static void build() {
        dp = act.getResources().getDisplayMetrics().density;

        // ---------- floating button ----------
        button = new TextView(act);
        button.setText("MOD");
        button.setTextColor(Color.WHITE);
        button.setTextSize(11);
        button.setTypeface(Typeface.DEFAULT_BOLD);
        button.setGravity(Gravity.CENTER);
        GradientDrawable oval = new GradientDrawable();
        oval.setShape(GradientDrawable.OVAL);
        oval.setColor(0xCC1E88E5);
        oval.setStroke(px(2), 0xFFFFFFFF);
        button.setBackground(oval);

        int size = px(44);
        btnLp = new FrameLayout.LayoutParams(size, size, Gravity.TOP | Gravity.LEFT);
        btnLp.leftMargin = getInt("bx", px(8));
        btnLp.topMargin = getInt("by", px(80));
        clampButton();
        button.setOnTouchListener(new View.OnTouchListener() {
            float downX, downY;
            int startL, startT;
            boolean dragging;

            public boolean onTouch(View v, MotionEvent e) {
                switch (e.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        downX = e.getRawX();
                        downY = e.getRawY();
                        startL = btnLp.leftMargin;
                        startT = btnLp.topMargin;
                        dragging = false;
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx = e.getRawX() - downX;
                        float dy = e.getRawY() - downY;
                        if (!dragging && (Math.abs(dx) > px(6) || Math.abs(dy) > px(6))) dragging = true;
                        if (dragging) {
                            btnLp.leftMargin = startL + (int) dx;
                            btnLp.topMargin = startT + (int) dy;
                            clampButton();
                            button.setLayoutParams(btnLp);
                            if (panel.getVisibility() == View.VISIBLE) placePanel();
                        }
                        return true;
                    case MotionEvent.ACTION_UP:
                        if (dragging) {
                            putInt("bx", btnLp.leftMargin);
                            putInt("by", btnLp.topMargin);
                        } else {
                            togglePanel();
                        }
                        return true;
                }
                return true;
            }
        });

        // ---------- panel ----------
        panel = new LinearLayout(act);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(px(12), px(10), px(12), px(12));
        panel.setBackground(rounded(0xEE1B1B1F, 12));
        panel.setClickable(true); // swallow taps so they do not reach the game

        LinearLayout head = new LinearLayout(act);
        head.setOrientation(LinearLayout.HORIZONTAL);
        head.setGravity(Gravity.CENTER_VERTICAL);
        TextView title = new TextView(act);
        title.setText("Stable Mod");
        title.setTextColor(Color.WHITE);
        title.setTextSize(15);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        head.addView(title, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        TextView close = new TextView(act);
        close.setText("X");
        close.setTextColor(0xFFBBBBBB);
        close.setTextSize(16);
        close.setTypeface(Typeface.DEFAULT_BOLD);
        close.setPadding(px(10), 0, px(2), 0);
        close.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                togglePanel();
            }
        });
        head.addView(close);
        panel.addView(head);

        // training multiplier
        multLabel = new TextView(act);
        multLabel.setTextColor(Color.WHITE);
        multLabel.setTextSize(13);
        multLabel.setPadding(0, px(10), 0, 0);
        panel.addView(multLabel);

        SeekBar bar = new SeekBar(act);
        bar.setMax(19); // progress 0..19 -> x1..x20
        int cur = clamp(getInt("mult", 1), 1, 20);
        bar.setProgress(cur - 1);
        setMultLabel(cur);
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                int m = p + 1;
                setMultLabel(m);
                if (nativeOk) nSetMult(m);
                putInt("mult", m);
            }

            public void onStartTrackingTouch(SeekBar s) {
            }

            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        panel.addView(bar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // fatigue remover
        Switch sw = new Switch(act);
        sw.setText("Remove fatigue");
        sw.setTextColor(Color.WHITE);
        sw.setTextSize(13);
        sw.setChecked(getInt("nofat", 0) == 1);
        sw.setPadding(0, px(8), 0, px(4));
        sw.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton b, boolean on) {
                if (nativeOk) nSetNoFatigue(on ? 1 : 0);
                putInt("nofat", on ? 1 : 0);
            }
        });
        panel.addView(sw);

        TextView hint = new TextView(act);
        hint.setText("Multiplier applies to training stat gains only. Fatigue stays at 0 while the switch is on.");
        hint.setTextColor(0xFF9E9E9E);
        hint.setTextSize(10);
        panel.addView(hint);

        statusLabel = new TextView(act);
        statusLabel.setTextSize(10);
        statusLabel.setPadding(0, px(6), 0, 0);
        panel.addView(statusLabel);

        panelLp = new FrameLayout.LayoutParams(px(250), ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.LEFT);
        panel.setVisibility(View.GONE);

        act.addContentView(button, btnLp);
        act.addContentView(panel, panelLp);
        updateStatus();
    }

    private static int clamp(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    private static void setMultLabel(int m) {
        multLabel.setText("Training multiplier: x" + m);
    }

    private static void clampButton() {
        int[] s = new int[2];
        screenSize(s);
        int size = btnLp.width;
        btnLp.leftMargin = clamp(btnLp.leftMargin, 0, Math.max(0, s[0] - size));
        btnLp.topMargin = clamp(btnLp.topMargin, 0, Math.max(0, s[1] - size));
    }

    private static void placePanel() {
        int[] s = new int[2];
        screenSize(s);
        panel.measure(View.MeasureSpec.makeMeasureSpec(px(250), View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        int pw = px(250);
        int ph = panel.getMeasuredHeight();
        int left = btnLp.leftMargin;
        int top = btnLp.topMargin + btnLp.height + px(6);
        if (left + pw > s[0]) left = s[0] - pw;
        if (left < 0) left = 0;
        if (top + ph > s[1]) top = btnLp.topMargin - ph - px(6);
        if (top < 0) top = 0;
        panelLp.leftMargin = left;
        panelLp.topMargin = top;
        panel.setLayoutParams(panelLp);
    }

    private static void togglePanel() {
        if (panel.getVisibility() == View.VISIBLE) {
            panel.setVisibility(View.GONE);
        } else {
            placePanel();
            panel.setVisibility(View.VISIBLE);
            panel.bringToFront();
            button.bringToFront();
            handler.removeCallbacks(ticker);
            handler.post(ticker);
        }
    }

    private static void updateStatus() {
        if (statusLabel == null) return;
        if (!nativeOk) {
            statusLabel.setText("Hooks: native library missing");
            statusLabel.setTextColor(0xFFEF5350);
            return;
        }
        int st = nGetState();
        int state = st & 15;
        int fail = st >> 4;
        if (state == 1) {
            statusLabel.setText("Hooks: active");
            statusLabel.setTextColor(0xFF66BB6A);
        } else if (state == 2) {
            statusLabel.setText("Hooks: FAILED (code " + fail + ") - different game version?");
            statusLabel.setTextColor(0xFFEF5350);
        } else {
            statusLabel.setText("Hooks: waiting for game...");
            statusLabel.setTextColor(0xFFFFCA28);
        }
    }
}
