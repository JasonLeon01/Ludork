package com.ludork.android;

import android.app.AlertDialog;
import android.text.InputType;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;

final class LudorkTextInputDialog {
    private final LudorkActivity activity;
    private AlertDialog dialog;
    private EditText input;
    private long sessionId;

    LudorkTextInputDialog(LudorkActivity activity) {
        this.activity = activity;
    }

    void show(long id, String text, String title, String prompt,
              String placeholder, String confirmText, String cancelText) {
        finish(false, false);
        if (!activity.isTextInputAvailable()) {
            LudorkActivity.completeTextInput(id, false, "");
            return;
        }
        sessionId = id;
        input = new EditText(activity);
        input.setSingleLine(true);
        input.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        input.setImeOptions(EditorInfo.IME_ACTION_DONE);
        input.setText(text);
        input.setHint(placeholder);
        input.setSelection(input.length());
        int padding = Math.round(24 * activity.getResources().getDisplayMetrics().density);
        LinearLayout content = new LinearLayout(activity);
        content.setPadding(padding, 0, padding, 0);
        content.addView(input, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        dialog = new AlertDialog.Builder(activity)
                .setTitle(title)
                .setMessage(prompt)
                .setView(content)
                .setPositiveButton(confirmText, (ignored, which) -> finish(true, true))
                .setNegativeButton(cancelText, (ignored, which) -> finish(false, true))
                .setOnCancelListener(ignored -> finish(false, true))
                .create();
        input.setOnEditorActionListener((view, action, event) -> {
            if (action == EditorInfo.IME_ACTION_DONE) {
                finish(true, true);
                return true;
            }
            return false;
        });
        dialog.setOnShowListener(ignored -> {
            if (input != null) {
                input.requestFocus();
            }
        });
        Window window = dialog.getWindow();
        if (window != null) {
            window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                    | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }
        dialog.show();
    }

    void dismiss(long id) {
        if (sessionId == id) {
            finish(false, false);
        }
    }

    boolean cancel() {
        if (sessionId == 0) {
            return false;
        }
        finish(false, true);
        return true;
    }

    private void finish(boolean accepted, boolean notify) {
        if (sessionId == 0) {
            return;
        }
        long previousId = sessionId;
        String text = accepted && input != null ? input.getText().toString() : "";
        AlertDialog previous = dialog;
        sessionId = 0;
        dialog = null;
        input = null;
        if (previous != null) {
            previous.setOnCancelListener(null);
            previous.setOnShowListener(null);
            previous.dismiss();
        }
        if (notify) {
            LudorkActivity.completeTextInput(previousId, accepted, text);
        }
    }
}
