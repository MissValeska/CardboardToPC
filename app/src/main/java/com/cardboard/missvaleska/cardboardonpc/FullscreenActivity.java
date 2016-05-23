package com.cardboard.missvaleska.cardboardonpc;

import android.app.ActionBar;
import android.app.Activity;
import android.app.PendingIntent;
import android.app.TaskStackBuilder;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.view.View;
import android.widget.TextView;
import android.widget.EditText;

public class FullscreenActivity extends Activity{

    private TextView tv;

    private EditText ipAddress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_fullscreen);
        hideActionBar();

        final View controlsView = findViewById(R.id.fullscreen_content_controls);
        final View contentView = findViewById(R.id.fullscreen_content);

        ipAddress = (EditText) findViewById(R.id.ip_text_feild);
    }

    private void hideUI(){

        View decorView = getWindow().getDecorView();

        // Hide the status bar.
        int uiOptions = View.SYSTEM_UI_FLAG_FULLSCREEN;
        decorView.setSystemUiVisibility(uiOptions);

        hideActionBar();
    }

    private void hideActionBar(){
        ActionBar actionBar = getActionBar();
        actionBar.hide();
    }

    public void switchToStreamView(View view){
        // Intent for the activity to open when user selects the notification
        Intent streamIntent = new Intent(this, ViewStreamActivity.class);
        streamIntent.putExtra("IP Address", ipAddress.getText().toString());

        // Use TaskStackBuilder to build the back stack and get the PendingIntent
        PendingIntent pendingIntent =
                TaskStackBuilder.create(this)
                        // add all of DetailsActivity's parents to the stack,
                        // followed by DetailsActivity itself
                        .addNextIntentWithParentStack(streamIntent)
                        .getPendingIntent(0, PendingIntent.FLAG_UPDATE_CURRENT);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this);
        builder.setContentIntent(pendingIntent);

        startActivity(streamIntent);
    }
}
