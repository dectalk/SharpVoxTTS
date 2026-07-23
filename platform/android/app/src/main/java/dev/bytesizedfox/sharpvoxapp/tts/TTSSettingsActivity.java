package dev.bytesizedfox.sharpvoxapp.tts;

import android.content.Intent;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;

import dev.bytesizedfox.sharpvoxapp.App;
import dev.bytesizedfox.sharpvoxapp.MainActivity;

public class TTSSettingsActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent myIntent = new Intent(this, MainActivity.class);
        App.openSettings = true;
        startActivity(myIntent);
        finish();
    }
}
