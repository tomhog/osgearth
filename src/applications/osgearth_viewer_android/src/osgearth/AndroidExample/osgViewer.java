package osgearth.AndroidExample;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PointF;
import android.os.Bundle;
import android.util.FloatMath;
import android.util.Log;
import android.view.GestureDetector;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.WindowManager;
import android.view.View.OnClickListener;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ImageButton;

import java.io.File;

public class osgViewer extends Activity implements View.OnTouchListener, View.OnKeyListener {
	
	private static final String TAG = "OSG Activity";
	//Ui elements
    EGLview mView;
    Button uiCenterViewButton;
    
    GestureDetector gestureDetector;
    static int tapcount;


    //Main Android Activity life cycle
    @Override protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        setContentView(R.layout.ui_layout_gles);
        
        //get gl view
	    mView= (EGLview) findViewById(R.id.surfaceGLES);
		mView.setOnTouchListener(this);
		mView.setOnKeyListener(this);
	    
		//get center camera button
	    uiCenterViewButton = (Button) findViewById(R.id.uiButtonCenter);
	    uiCenterViewButton.setOnClickListener(uiListenerCenterView);
	   
	    //create gesture detector for double tap
	    Context context = getApplicationContext();    	
	    gestureDetector = new GestureDetector(context, new GestureListener());
	   
    }
    @Override protected void onPause() {
        super.onPause();
        mView.onPause();
    }
    @Override protected void onResume() {
        super.onResume();
        mView.onResume();
    }
    
    //Main view event processing
    @Override
	public boolean onKey(View v, int keyCode, KeyEvent event) {
		
		return true;
	}
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event){
    	//DO NOTHING this will render useless every menu key except Home
    	int keyChar= event.getUnicodeChar();
    	osgNativeLib.keyboardDown(keyChar);
    	return true;
    }
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event){
    	switch (keyCode){
    	case KeyEvent.KEYCODE_BACK:
    		super.onDestroy();
    		this.finish();
    		break;
    	case KeyEvent.KEYCODE_SEARCH:
    		break;
    	case KeyEvent.KEYCODE_MENU:
    		this.openOptionsMenu();
    		break;
    	default:
    		int keyChar= event.getUnicodeChar();
    		osgNativeLib.keyboardUp(keyChar);    		
    	}
    	
    	return true;
    }
    @Override
    public boolean onTouch(View v, MotionEvent event) {
    	
    	gestureDetector.onTouchEvent(event);
    	
    	//dumpEvent(event);
    	long time_arrival = event.getEventTime();
    	int n_points = event.getPointerCount();
    	int action = event.getAction() & MotionEvent.ACTION_MASK;
    	
    		switch(action){
    		case MotionEvent.ACTION_DOWN:
    		case MotionEvent.ACTION_POINTER_DOWN:
    			for(int i=0; i<n_points; i++){
    				int touchID = event.getPointerId(i);
    				osgNativeLib.touchBeganEvent(touchID, event.getX(i), event.getY(i));
    			}
    			break;
    		case MotionEvent.ACTION_CANCEL:
    			for(int i=0; i<n_points; i++){
    				int touchID = event.getPointerId(i);
    				osgNativeLib.touchEndedEvent(touchID, event.getX(i), event.getY(i),1);
    			}
    			break;
    		case MotionEvent.ACTION_MOVE:
    		//case MotionEvent.ACTION_POINTER_MOVE:
    			final int historySize = event.getHistorySize();
    		    if(n_points > 1){ 
    		    	for (int h = 0; h < historySize; h++) {
    		    		//System.out.printf("At time %d:", ev.getHistoricalEventTime(h));
    		    		for (int p = 0; p < n_points; p++) {
    		    			int touchID = event.getPointerId(p);
    		    			osgNativeLib.touchMovedEvent(touchID, event.getHistoricalX(p, h), event.getHistoricalY(p, h));
    		    		}
    		    	}
    		    }
    			for(int i=0; i<n_points; i++){
    			    Log.d(TAG,  "y: " + event.getY(i));
    				int touchID = event.getPointerId(i);
    				osgNativeLib.touchMovedEvent(touchID, event.getX(i), event.getY(i));
    			}
    			break;
    		case MotionEvent.ACTION_UP:
    		case MotionEvent.ACTION_POINTER_UP:
    			for(int i=0; i<n_points; i++){
    				int touchID = event.getPointerId(i);
    				osgNativeLib.touchEndedEvent(touchID, event.getX(i), event.getY(i), tapcount);
    				tapcount = 0;
    			}
    			break;
    		default :
    			Log.e(TAG,"1 point Action not captured");	
    		}		
			
		return true;
	}

    private class GestureListener extends GestureDetector.SimpleOnGestureListener {

        @Override
        public boolean onDown(MotionEvent event) {
            return true;
        }
        // event when double tap occurs
        @Override
        public boolean onDoubleTap(MotionEvent event) {
            float x = event.getX();
            float y = event.getY();

            int touchID = event.getPointerId(0);
            //osgNativeLib.clearEventQueue();
			//osgNativeLib.touchEndedEvent(touchID, event.getX(0), event.getY(0), 2);
			tapcount = 2;
            return true;
        }
    }
    
    //Ui Listeners
    OnClickListener uiListenerCenterView = new OnClickListener() {
        public void onClick(View v) {
        	//Log.d(TAG, "Center View");
        	osgNativeLib.keyboardDown(32);
        	osgNativeLib.keyboardUp(32);
        }
    };

    
    //Menu

    
    //Android Life Cycle Menu
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.appmenu, menu);
        return super.onCreateOptionsMenu(menu);
    }
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
        case R.id.menuLoadObject:
        	Log.d(TAG,"Load Object");
        	//loadLayerAddress.show();
            return true;
        case R.id.menuCleanScene:
        	Log.d(TAG,"Clean Scene");
        	//osgNativeLib.clearContents();
            return true;
        case R.id.menuDeleteObject:
        	Log.d(TAG,"Delete a object");
        	/*String vNames[] = osgNativeLib.getObjectNames();
        	
        	//Remove Layer Dialog
    		AlertDialog.Builder removeLayerDialogBuilder = new AlertDialog.Builder(this);
    		removeLayerDialogBuilder.setTitle(R.string.uiDialogTextChoseRemove);
    		removeLayerDialogBuilder.setItems(vNames, new DialogInterface.OnClickListener() {
    			
    			@Override
    			public void onClick(DialogInterface dialog, int witch) {
    				// TODO Auto-generated method stub
    				osgNativeLib.unLoadObject(witch);
    			}
    		});
    		removeLayerDialog = removeLayerDialogBuilder.create();

    		if(vNames.length > 0)
    			removeLayerDialog.show();
        	*/
            return true;
        case R.id.menuChangeBackground:
        	Log.d(TAG,"Change background color");
        	int[] test = new int [3];
        	/*test = osgNativeLib.getClearColor();
        	backgroundColor = Color.rgb(test[0], test[1], test[2]);
        	
        	ColorPickerDialog colorDialog;
        	new ColorPickerDialog(this, this, backgroundColor).show();
        	*/
            return true;
        case R.id.menuShowKeyboard:
        	Log.d(TAG,"Keyboard");
        	InputMethodManager mgr= (InputMethodManager)this.getSystemService(Context.INPUT_METHOD_SERVICE);
    		mgr.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
            return true;
        default:
            return super.onOptionsItemSelected(item);
        }
    }
}