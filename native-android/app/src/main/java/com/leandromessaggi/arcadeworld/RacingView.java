package com.leandromessaggi.arcadeworld;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * A new Android arcade racer. All gameplay, road geometry, art and physics are
 * independently implemented in Java/Android Canvas. It is NOT an N64 recompiler,
 * reverse-engineered port or a copy of Nintendo/Midway game code or game assets.
 */
public final class RacingView extends SurfaceView implements SurfaceHolder.Callback, Runnable {
    private static final int MENU = 0, RACING = 1, PAUSED = 2, FINISHED = 3, CONTROLS = 4;
    private static final int W = 1280, H = 720;
    private static final int SEGMENT = 180, SEGMENTS = 420;
    private static final double TRACK_LENGTH = SEGMENT * (double) SEGMENTS;
    private static final double ROAD_HALF_WIDTH = 950.0, CAMERA_HEIGHT = 1000.0;
    private static final double PLAYER_Z = 1150.0, DRAW_DISTANCE = 130.0 * SEGMENT;
    private static final int WHITE = 0xfff3f8ff, NAVY = 0xff091a30, GLASS = 0xff102b49;
    private static final String[] TRACKS = {"COSTA SOLAR", "CIDADE NEON", "DESERTO VERMELHO", "PICOS GELADOS"};
    private static final String[] TRACK_DESCRIPTIONS = {
            "LITORAL • COQUEIROS • PÔR DO SOL", "MEGACIDADE • LUZES • TÚNEIS",
            "DUNAS • CACTOS • CALOR", "MONTANHAS • PINHEIROS • NEVE"};
    private static final int[] SKIES = {0xff72c9df,0xff152448,0xffee9c58,0xff62abc9};
    private static final int[] GROUNDS = {0xff319476,0xff34415b,0xffc3824c,0xff80968d};
    private static final int[] ROAD_COLORS = {0xff424b59,0xff343b53,0xff69544b,0xff474d56};
    private static final String[] ACTION_LABELS = {"VIRAR ESQUERDA", "VIRAR DIREITA", "ACELERAR", "FREAR", "NITRO", "PAUSAR"};
    private static final int[] DEFAULT_KEYS = {KeyEvent.KEYCODE_DPAD_LEFT,KeyEvent.KEYCODE_DPAD_RIGHT,
            KeyEvent.KEYCODE_BUTTON_A,KeyEvent.KEYCODE_BUTTON_B,KeyEvent.KEYCODE_BUTTON_X,
            KeyEvent.KEYCODE_BUTTON_START};
    private static final int[] CAR_COLORS = {0xff40bfff, 0xffffbf52, 0xffc86aff, 0xffe45c69, 0xff76d68e, 0xffeadde2};

    private final SharedPreferences prefs;
    private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF tempRect = new RectF();
    private final ArrayList<Rival> rivals = new ArrayList<>();
    private final Set<Integer> pressedKeys = new HashSet<>();
    private final ArrayList<TouchPoint> touches = new ArrayList<>();
    private final Object stateLock = new Object();
    private volatile boolean running, externalPaused;
    private Thread thread;
    private int screen = MENU, trackIndex = 0, selectedTrack = 0;
    private int lastSelection = 0;
    private double distance, speed, lane, nitro = 100, raceTime, countdown;
    private double collisionFlash, shake, engineTick, checkpointFlash, finishTime;
    private boolean countdownBeeped;
    private float axisSteering, axisGas, axisBrake;
    private int remapAction = -1;
    private final int[] mapping = DEFAULT_KEYS.clone();
    private ToneGenerator tones;
    private boolean audioOn;

    private static final class Rival {
        double z, lane, velocity, phase;
        int color;
        Rival(double z, double lane, double velocity, int color, double phase) {
            this.z=z; this.lane=lane; this.velocity=velocity; this.color=color; this.phase=phase;
        }
    }
    private static final class TouchPoint {
        int id; float x,y;
        TouchPoint(int id,float x,float y) { this.id=id;this.x=x;this.y=y; }
    }
    private static final class Projection {
        float x,y,halfWidth;
        Projection(float x,float y,float hw) { this.x=x;this.y=y;this.halfWidth=hw; }
    }

    public RacingView(Context context) {
        super(context);
        prefs=context.getSharedPreferences("arcade_native_v1",Context.MODE_PRIVATE);
        trackIndex = Math.max(0, Math.min(TRACKS.length-1,prefs.getInt("last_track",0)));
        audioOn = prefs.getBoolean("audio_on",true);
        for(int i=0;i<mapping.length;i++) mapping[i]=prefs.getInt("map_"+i, DEFAULT_KEYS[i]);
        try { tones = new ToneGenerator(AudioManager.STREAM_MUSIC,35); }
        catch (RuntimeException ignored) { tones=null; }
        getHolder().addCallback(this);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public void shutdown() {
        running=false;
        stopThread();
        if (tones != null) { tones.release(); tones=null; }
    }
    public void pauseGame() { externalPaused=true; }
    public void resumeGame() { externalPaused=false; }
    @Override public void surfaceCreated(SurfaceHolder holder) {
        running = true;
        thread=new Thread(this,"ArcadeWorldRenderer");
        thread.start();
    }
    @Override public void surfaceChanged(SurfaceHolder holder,int format,int width,int height) { }
    @Override public void surfaceDestroyed(SurfaceHolder holder) { running=false; stopThread(); }
    private void stopThread() {
        Thread t=thread; thread=null;
        if(t!=null && t!=Thread.currentThread()) {
            try { t.join(1400); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }
    }
    @Override public void run() {
        long previous=System.nanoTime();
        while (running) {
            long now=System.nanoTime();
            double dt=Math.min(0.04,(now-previous)/1_000_000_000.0);
            previous=now;
            if (!externalPaused) {
                synchronized(stateLock) { update(dt); }
            }
            Canvas canvas=null;
            try {
                if (getHolder().getSurface().isValid()) canvas=getHolder().lockCanvas();
                if(canvas!=null) synchronized(stateLock) { render(canvas); }
            } catch (RuntimeException ignored) {
            } finally {
                if(canvas!=null) try {getHolder().unlockCanvasAndPost(canvas);} catch(RuntimeException ignored) { }
            }
            try { Thread.sleep(12); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); break; }
        }
    }

    private void tone(int which,int millis) {
        if (!audioOn || tones==null) return;
        try { tones.startTone(which,millis); } catch (RuntimeException ignored) { }
    }
    private void startRace() {
        screen=RACING; selectedTrack=trackIndex;
        prefs.edit().putInt("last_track",trackIndex).apply();
        distance=0;speed=0;lane=0;nitro=100;raceTime=0;countdown=3.4;
        collisionFlash=0;shake=0;checkpointFlash=0;finishTime=0;
        countdownBeeped=false; rivals.clear(); pressedKeys.clear(); touches.clear();
        for(int i=0;i<9;i++) {
            double z=2200.0+i*6500.0;
            rivals.add(new Rival(z, -0.7+(i%3)*0.69, 390+i%4*46,
                    CAR_COLORS[i%CAR_COLORS.length], i*1.83));
        }
        tone(ToneGenerator.TONE_PROP_BEEP,170);
    }
    private boolean touch(int code) {
        for(TouchPoint t:touches) {
            float x=t.x,y=t.y;
            if(code==0 && x<215 && y>430) return true;
            if(code==1 && x>=220 && x<425 && y>430) return true;
            if(code==2 && x>1030 && y>395) return true;
            if(code==3 && x>835 && x<1032 && y>538) return true;
            if(code==4 && x>845 && x<1035 && y>390 && y<535) return true;
        }
        return false;
    }
    private boolean keyAction(int a) {
        return pressedKeys.contains(mapping[a]);
    }
    private void update(double dt) {
        if(screen!=RACING) return;
        collisionFlash=Math.max(0,collisionFlash-dt*2.2);
        shake=Math.max(0,shake-dt*4.8);
        checkpointFlash=Math.max(0,checkpointFlash-dt*1.7);
        if(countdown>0) {
            int was=(int)Math.ceil(countdown);
            countdown-=dt;
            int is=(int)Math.ceil(Math.max(0,countdown));
            if(was!=is) tone(ToneGenerator.TONE_PROP_BEEP,countdown<=0?300:130);
            return;
        }
        raceTime+=dt;
        boolean gas=touch(2)||keyAction(2)||axisGas>0.22;
        boolean brake=touch(3)||keyAction(3)||axisBrake>0.22;
        boolean boost=(touch(4)||keyAction(4)) && nitro>0.25 && speed>110;
        float steer=axisSteering;
        if(touch(0)||keyAction(0)) steer-=1;
        if(touch(1)||keyAction(1)) steer+=1;
        steer=Math.max(-1,Math.min(1,steer));
        double top=boost?940:755;
        if(gas) speed+=490*dt;
        else speed-=155*dt;
        if(brake) speed-=850*dt;
        if(boost) {speed+=780*dt; nitro=Math.max(0,nitro-27*dt);}
        else nitro=Math.min(100,nitro+8.8*dt);
        if(speed>top) speed-=(speed-top)*dt*3.5;
        speed=Math.max(0,Math.min(980,speed));
        lane+=steer*dt*(0.65+speed/590.0);
        if(lane>1.45)lane=1.45;
        if(lane< -1.45)lane=-1.45;
        if(Math.abs(lane)>0.92) speed=Math.max(0,speed-285*dt);
        double previous=distance;
        distance+=speed*dt;
        for(Rival r:rivals) {
            r.z=(r.z+r.velocity*dt)%TRACK_LENGTH;
            r.phase+=dt;
            r.lane+=Math.sin(r.phase*0.8)*dt*0.06;
            r.lane=Math.max(-0.8,Math.min(0.8,r.lane));
            double myZ=distance%TRACK_LENGTH;
            double dz=Math.abs(r.z-myZ);
            dz=Math.min(dz,TRACK_LENGTH-dz);
            if(dz<170 && Math.abs(r.lane-lane)<0.32 && collisionFlash<0.10) {
                collisionFlash=1;shake=1; speed*=0.55;
                tone(ToneGenerator.TONE_PROP_NACK,200);
            }
        }
        int oldGate=(int)(previous/(TRACK_LENGTH/4));
        int newGate=(int)(distance/(TRACK_LENGTH/4));
        if(newGate>oldGate && newGate%4!=0) {checkpointFlash=1; tone(ToneGenerator.TONE_PROP_ACK,160);}
        if(distance>=TRACK_LENGTH*3) {
            finishTime=raceTime; speed=0;screen=FINISHED;
            String k="record_"+selectedTrack;
            long old=prefs.getLong(k,Long.MAX_VALUE);
            long ms=Math.round(raceTime*1000);
            if(ms<old) prefs.edit().putLong(k,ms).apply();
            tone(ToneGenerator.TONE_PROP_ACK,400);
        }
        engineTick+=dt;
    }

    private double progress(double z) { return (((z%TRACK_LENGTH)+TRACK_LENGTH)%TRACK_LENGTH)/TRACK_LENGTH; }
    private double centerAt(double z) {
        double t=progress(z)*Math.PI*2;
        double amount=new double[] {1600,2350,1900,2600}[selectedTrack];
        return Math.sin(t*3+0.3)*amount + Math.sin(t*7+1.1)*470
                +Math.cos(t*2-0.5)*560;
    }
    private double heightAt(double z) {
        double t=progress(z)*Math.PI*2;
        return Math.sin(t*4)*240 + Math.sin(t*9)*95 + Math.sin(t*2)*105;
    }
    private Projection project(double worldZ, double worldX, double elevation, double cameraX) {
        double depth=worldZ-(distance-PLAYER_Z);
        double scale=1.11/Math.max(1,depth);
        double cameraY=heightAt(distance)+CAMERA_HEIGHT;
        float x=(float)((1+scale*(worldX-cameraX))*W*0.5);
        float y=(float)((1-scale*(elevation-cameraY))*H*0.5);
        float hw=(float)(scale*ROAD_HALF_WIDTH*W*0.5);
        return new Projection(x,y,hw);
    }
    private void fill(Canvas c,int color) { c.drawColor(color); }
    private void rectangle(Canvas c,float l,float t,float r,float b,int color) {
        if(b<t || r<l)return;
        p.setColor(color);p.setStyle(Paint.Style.FILL);p.setShader(null);
        c.drawRect(l,t,r,b,p);
    }
    private void oval(Canvas c,float l,float t,float r,float b,int color) {
        p.setColor(color);p.setStyle(Paint.Style.FILL);p.setShader(null);
        c.drawOval(l,t,r,b,p);
    }
    private void round(Canvas c,float l,float t,float r,float b,float radius,int color) {
        p.setColor(color);p.setStyle(Paint.Style.FILL);p.setShader(null);
        tempRect.set(l,t,r,b);c.drawRoundRect(tempRect,radius,radius,p);
    }
    private void line(Canvas c,float x,float y,float x2,float y2,int color,float width) {
        p.setColor(color);p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(width);
        p.setStrokeCap(Paint.Cap.ROUND);p.setShader(null);c.drawLine(x,y,x2,y2,p);p.setStyle(Paint.Style.FILL);
    }
    private void poly(Canvas c,int color,float... pts) {
        if(pts.length<6)return;
        p.setColor(color);p.setStyle(Paint.Style.FILL);p.setShader(null);
        Path path=new Path();path.moveTo(pts[0],pts[1]);
        for(int i=2;i<pts.length;i+=2)path.lineTo(pts[i],pts[i+1]);
        path.close();c.drawPath(path,p);
    }
    private void text(Canvas c,String s,float x,float y,float size,int color,boolean bold,Paint.Align align) {
        p.setShader(null);p.setColor(color);p.setStyle(Paint.Style.FILL);p.setTextSize(size);
        p.setTypeface(bold?android.graphics.Typeface.create("sans-serif-condensed",android.graphics.Typeface.BOLD)
                :android.graphics.Typeface.create("sans-serif",android.graphics.Typeface.NORMAL));
        p.setTextAlign(align);p.setShadowLayer(0,0,0,0);c.drawText(s,x,y,p);
    }
    private void centered(Canvas c,String s,float x,float y,float size,int color,boolean bold) {
        text(c,s,x,y,size,color,bold,Paint.Align.CENTER);
    }
    private static int lighten(int color,float k) {
        int r=Math.min(255,Math.max(0,(int)(Color.red(color)*k)));
        int g=Math.min(255,Math.max(0,(int)(Color.green(color)*k)));
        int b=Math.min(255,Math.max(0,(int)(Color.blue(color)*k)));
        return Color.rgb(r,g,b);
    }
    private void render(Canvas actual) {
        actual.save();
        actual.scale(actual.getWidth()/(float)W,actual.getHeight()/(float)H);
        if(screen==MENU) {drawMenu(actual);}
        else if(screen==CONTROLS) { drawControls(actual); }
        else {
            drawRace(actual);
            if(screen==PAUSED) drawPause(actual);
            if(screen==FINISHED) drawFinish(actual);
        }
        actual.restore();
    }

    private void drawBackground(Canvas c) {
        int sky=SKIES[selectedTrack],ground=GROUNDS[selectedTrack];
        p.setColor(WHITE);p.setStyle(Paint.Style.FILL);
        p.setShader(new LinearGradient(0,0,0,495,lighten(sky,0.65f),sky,Shader.TileMode.CLAMP));
        c.drawRect(0,0,W,495,p);p.setShader(null);
        rectangle(c,0,400,W,H,ground);
        if(selectedTrack==1) {
            oval(c,980,72,1050,142,0xffb5c9f9);
            for(int i=0;i<36;i++) {
                float x=i*49-12f;
                float h=75+(i*47%135);
                rectangle(c,x,404-h,x+45,410,0xff182b48);
                for(int j=0;j<5;j++)for(int k=0;k<2;k++){
                    if((i+j+k)%3!=0)rectangle(c,x+9+k*16,407-h+12+j*24,x+15+k*16,407-h+21+j*24,0xff5082b5);
                }
            }
        } else {
            int sun=selectedTrack==2?0xffffd086:0xfffff4bc;
            oval(c,960,90,1070,200,sun);
            int hill=selectedTrack==0?0xff3a8d92:selectedTrack==2?0xffb46b48:0xff7797a8;
            for(int k=0;k<10;k++) {
                int x=k*155-140;
                int top=310+(k*89%90);
                poly(c,hill,x,405,x+98,top,x+230,405);
                if(selectedTrack==3)
                    poly(c,0xffe0eff8,x+78,top+26,x+98,top,x+123,top+40);
            }
        }
    }
    private void drawRoad(Canvas c) {
        final double cameraX=centerAt(distance)+lane*ROAD_HALF_WIDTH;
        int first=(int)Math.floor(distance/SEGMENT);
        for(int i=129;i>=0;i--) {
            int index=first+i;
            double a=index*(double)SEGMENT,b=(index+1.0)*SEGMENT;
            Projection near=project(a,centerAt(a),heightAt(a),cameraX);
            Projection far=project(b,centerAt(b),heightAt(b),cameraX);
            if(far.y>720 || near.y < 0 || far.y>near.y)continue;
            float ya=Math.max(-1,far.y),yb=Math.min(720,near.y);
            boolean alt=((index/3)&1)==0;
            rectangle(c,0,ya,1280,yb,lighten(GROUNDS[selectedTrack],alt?1.06f:0.94f));
            float sa=far.halfWidth,sb=near.halfWidth;
            // Shoulder, road and center lines, all drawn as perspective polygons.
            int shoulder=alt?0xffeff5eb:0xffed5548;
            poly(c,shoulder, far.x-sa*1.17f,far.y,far.x+sa*1.17f,far.y,
                    near.x+sb*1.17f,near.y,near.x-sb*1.17f,near.y);
            int asphalt=lighten(ROAD_COLORS[selectedTrack],alt?1.09f:0.96f);
            poly(c,asphalt,far.x-sa,far.y,far.x+sa,far.y,near.x+sb,near.y,near.x-sb,near.y);
            if((index/3)%2==0) {
                for(int laneMark=-1;laneMark<=1;laneMark+=2) {
                    float a1=far.x+sa*laneMark*0.34f, b1=near.x+sb*laneMark*0.34f;
                    poly(c,0xffe2e8da,a1-sa*0.007f,far.y,a1+sa*0.007f,far.y,
                            b1+sb*0.007f,near.y,b1-sb*0.007f,near.y);
                }
            }
            if(i%7==0 && i>1) {
                drawRoadside(c,index,a,near,cameraX);
            }
            for(Rival r:rivals) {
                double relative=(r.z - distance%TRACK_LENGTH + TRACK_LENGTH)%TRACK_LENGTH;
                if(relative>0 && relative<DRAW_DISTANCE) {
                    double rz=distance+relative;
                    if(rz>=a && rz<b) {
                        Projection car=project(rz,centerAt(rz)+r.lane*ROAD_HALF_WIDTH,heightAt(rz),cameraX);
                        float factor=Math.min(1.35f,Math.max(.06f,car.halfWidth/575f));
                        drawCar(c,car.x,car.y-10*factor,120*factor,104*factor,r.color,false,0);
                    }
                }
            }
        }
    }
    private void drawRoadside(Canvas c,int index,double z,Projection position,double cameraX) {
        if(position.halfWidth<7 || position.halfWidth>600)return;
        for(int side=-1;side<=1;side+=2) {
            if((index+side)%5==0)continue;
            Projection s=project(z,centerAt(z)+side*ROAD_HALF_WIDTH*1.7,heightAt(z),cameraX);
            float scale=Math.min(4,Math.max(0.02f,s.halfWidth/560));
            float x=s.x, y=s.y;
            if(x<-200 || x>1500 || y>850)continue;
            if(selectedTrack==0) {
                line(c,x,y,x+15*scale,y-170*scale,0xff654933,14*scale);
                oval(c,x-54*scale,y-220*scale,x+72*scale,y-115*scale,0xff267d5e);
                oval(c,x-40*scale,y-231*scale,x+20*scale,y-155*scale,0xff55bf8b);
            } else if(selectedTrack==1) {
                rectangle(c,x-16*scale,y-130*scale,x+16*scale,y,0xff273e59);
                oval(c,x-18*scale,y-148*scale,x+18*scale,y-111*scale,0xfffdd17b);
                line(c,x-48*scale,y-140*scale,x+48*scale,y-140*scale,0xff5ebef5,4*scale);
            } else if(selectedTrack==2) {
                line(c,x,y,x,y-140*scale,0xff26795c,23*scale);
                line(c,x,y-88*scale,x-35*scale,y-108*scale,0xff26795c,18*scale);
                line(c,x-35*scale,y-108*scale,x-35*scale,y-143*scale,0xff26795c,16*scale);
                line(c,x,y-75*scale,x+34*scale,y-95*scale,0xff26795c,18*scale);
            } else {
                poly(c,0xff1d635d,x-58*scale,y-10*scale,x,y-200*scale,x+58*scale,y-10*scale);
                poly(c,0xff377e75,x-45*scale,y-70*scale,x,y-235*scale,x+45*scale,y-70*scale);
                poly(c,0xfff6faf9,x-13*scale,y-187*scale,x,y-235*scale,x+13*scale,y-187*scale);
                rectangle(c,x-7*scale,y-10*scale,x+7*scale,y,0xff534338);
            }
        }
    }
    private void drawCar(Canvas c,float cx,float bottom,float width,float height,int color,boolean player,float steer) {
        if(width<3)return;
        float l=cx-width*.5f,r=cx+width*.5f,top=bottom-height;
        oval(c,l-12,bottom-height*.08f,r+12,bottom+height*.07f,0x66050a13);
        round(c,l+width*.07f,top+height*.59f,l+width*.25f,bottom,9,0xff0b111b);
        round(c,r-width*.25f,top+height*.59f,r-width*.07f,bottom,9,0xff0b111b);
        poly(c,color,l+width*.11f,top+height*.51f,l+width*.22f,top+height*.12f,
                r-width*.22f,top+height*.12f,r-width*.11f,top+height*.51f,
                r,bottom-height*.2f,l,bottom-height*.2f);
        poly(c,0xff16334e,l+width*.27f,top+height*.2f,r-width*.27f,top+height*.2f,
                r-width*.19f,top+height*.53f,l+width*.19f,top+height*.53f);
        poly(c,lighten(color,1.22f),l+width*.12f,top+height*.55f,r-width*.12f,top+height*.55f,
                r-width*.06f,top+height*.78f,l+width*.06f,top+height*.78f);
        round(c,l+width*.1f,bottom-height*.33f,l+width*.31f,bottom-height*.19f,5,0xffff4d44);
        round(c,r-width*.31f,bottom-height*.33f,r-width*.1f,bottom-height*.19f,5,0xffff4d44);
        round(c,l+width*.35f,bottom-height*.3f,r-width*.35f,bottom-height*.24f,3,0xff152132);
        if(player) {
            round(c,l+width*.15f,top+height*.06f,r-width*.15f,top+height*.14f,4,0xff112840);
            text(c,"07",cx,top+height*.84f,27,WHITE,true,Paint.Align.CENTER);
        }
    }
    private void drawRace(Canvas c) {
        if(shake>0) { c.save();c.translate((float)Math.sin(raceTime*69)*shake*7,0); }
        drawBackground(c);
        drawRoad(c);
        drawCar(c,W*.5f+axisSteering*5,680,220,150,0xffff8743,true,axisSteering);
        if(shake>0) c.restore();
        if(collisionFlash>0) rectangle(c,0,0,W,H,Color.argb((int)(collisionFlash*90),255,42,22));
        drawHUD(c);
        drawTouch(c);
        if(countdown>0) {
            int second=(int)Math.ceil(countdown);
            centered(c,second>3?"PREPARE-SE":Integer.toString(second),W/2f,270,124,WHITE,true);
            centered(c,"ACELERE QUANDO O SINAL ABRIR",W/2f,328,29,0xffd9f0ff,false);
        }
        if(checkpointFlash>0) {
            centered(c,"CHECKPOINT!",640,170,49,0xffffe380,true);
        }
    }
    private void drawHUD(Canvas c) {
        round(c,23,18,359,132,18,0xd9081930);
        text(c,"ARCADE WORLD",43,55,28,0xff54e9f0,true,Paint.Align.LEFT);
        text(c,TRACKS[selectedTrack],43,89,21,WHITE,true,Paint.Align.LEFT);
        text(c,"VOLTAS  "+Math.min(3,1+(int)(distance/TRACK_LENGTH))+" / 3",43,118,18,0xffd7ecf8,true,Paint.Align.LEFT);
        round(c,438,14,832,92,14,0xd9081930);
        centered(c,formatTime(raceTime),636,71,54,WHITE,true);
        round(c,941,18,1258,139,18,0xd9081930);
        text(c,String.format(Locale.US,"%03d",(int)Math.round(speed*0.38)),1042,84,65,WHITE,true,Paint.Align.CENTER);
        text(c,"KM/H",1158,81,21,0xffa9c1da,true,Paint.Align.LEFT);
        text(c,"NITRO",960,116,20,0xff7ce5ef,true,Paint.Align.LEFT);
        round(c,1038,101,1229,121,7,0xff314665);
        round(c,1038,101,(float)(1038+191*nitro/100),121,7,0xff3de1d9);
        round(c,464,102,815,121,8,0xa610233e);
        rectangle(c,464,102,(float)(464+351*progress(distance)),121,0xfff9b44c);
        text(c,"☰",1231,177,48,WHITE,true,Paint.Align.CENTER);
    }
    private static String formatTime(double seconds) {
        int ms=(int)Math.round(seconds*1000);
        return String.format(Locale.US,"%02d:%02d.%02d",ms/60000,(ms/1000)%60,(ms%1000)/10);
    }
    private void touchButton(Canvas c,float left,float top,float right,float bottom,String label,int color,boolean active) {
        round(c,left,top,right,bottom,20,active?color:0x99172b4c);
        if(active)round(c,left+6,top+6,right-6,bottom-6,18,0x664be4fc);
        centered(c,label,(left+right)/2,(top+bottom)/2+14,37,WHITE,true);
    }
    private void drawTouch(Canvas c) {
        touchButton(c,28,538,195,698,"◀",0xff237c9e,touch(0));
        touchButton(c,215,538,382,698,"▶",0xff237c9e,touch(1));
        touchButton(c,856,401,1020,532,"NITRO",0xff145c75,touch(4));
        touchButton(c,856,548,1020,698,"FREIO",0xff7b4545,touch(3));
        touchButton(c,1045,444,1253,698,"GÁS",0xff3a9270,touch(2));
    }
    private void drawTitleBackground(Canvas c) {
        drawBackground(c);
        line(c,0,590,1280,590,0xff25476b,4);
        for(int i=0;i<8;i++) {
            float x=130+145*i;
            line(c,640,330,x,720,0x775ce6fa,3);
        }
        for(int i=0;i<8;i++) line(c,0,420+i*i*7,1280,420+i*i*7,0x3353b9db,2);
        drawCar(c,900,608,438,305,0xfffc8b44,true,0);
    }
    private void drawMenu(Canvas c) {
        selectedTrack=trackIndex;
        drawTitleBackground(c);
        text(c,"ARCADE",86,181,130,WHITE,true,Paint.Align.LEFT);
        text(c,"WORLD",86,307,145,0xff43dfeb,true,Paint.Align.LEFT);
        text(c,"RACING  ·  ANDROID NATIVO",92,354,28,0xfff8c16e,true,Paint.Align.LEFT);
        round(c,86,402,665,563,25,0xdd07172b);
        text(c,"PISTA  "+(trackIndex+1)+" / "+TRACKS.length,109,448,22,0xff72dce7,true,Paint.Align.LEFT);
        text(c,TRACKS[trackIndex],109,489,41,WHITE,true,Paint.Align.LEFT);
        text(c,TRACK_DESCRIPTIONS[trackIndex],111,526,18,0xffa9cbd8,false,Paint.Align.LEFT);
        round(c,88,586,442,665,16,0xff21b9a7);
        centered(c,"INICIAR CORRIDA",265,637,35,0xff062a36,true);
        round(c,455,586,665,665,16,0xff223a5b);
        centered(c,"CONTROLES",558,637,29,WHITE,true);
        round(c,560,409,656,463,12,0xff34577a);
        centered(c,"▶",610,448,30,WHITE,true);
        round(c,697,409,766,463,12,0xff34577a);
        centered(c,"◀",731,448,30,WHITE,true);
        String record=prefs.getLong("record_"+trackIndex,-1)<0?"SEM RECORDE":
            "RECORDE  "+formatTime(prefs.getLong("record_"+trackIndex,0)/1000.0);
        text(c,record,100,690,18,0xffffd887,true,Paint.Align.LEFT);
    }
    private void drawOverlay(Canvas c,String title,String sub) {
        rectangle(c,0,0,W,H,0xb8061024);
        round(c,260,134,1020,620,28,0xf0132948);
        centered(c,title,640,235,75,WHITE,true);
        centered(c,sub,640,296,25,0xffa6d8e8,false);
    }
    private void drawPause(Canvas c) {
        drawOverlay(c,"PAUSADO","CORRIDA EM ESPERA");
        round(c,365,352,914,433,20,0xff25c4ad);
        centered(c,"CONTINUAR",640,406,39,0xff08263c,true);
        round(c,365,453,914,533,20,0xff365779);
        centered(c,"CONTROLES",640,507,36,WHITE,true);
        round(c,365,555,914,605,18,0xff73545c);
        centered(c,"VOLTAR AO MENU",640,590,27,WHITE,true);
    }
    private void drawFinish(Canvas c) {
        drawOverlay(c,"CHEGADA!","VOCÊ TERMINOU AS 3 VOLTAS");
        centered(c,formatTime(finishTime),640,387,95,0xfff4d06f,true);
        long best=prefs.getLong("record_"+selectedTrack,Math.round(finishTime*1000));
        centered(c,"MELHOR TEMPO: "+formatTime(best/1000.0),640,435,27,0xffbcdbe7,false);
        round(c,380,490,900,570,18,0xff21b9a7);
        centered(c,"CORRER NOVAMENTE",640,544,35,NAVY,true);
        centered(c,"TOQUE FORA DO BOTÃO PARA O MENU",640,608,20,WHITE,false);
    }
    private void drawControls(Canvas c) {
        fill(c,0xff081629);
        rectangle(c,0,0,W,105,0xff123153);
        text(c,"PERSONALIZAR CONTROLES",58,73,58,WHITE,true,Paint.Align.LEFT);
        text(c,"TOQUE EM UMA AÇÃO E PRESSIONE O BOTÃO DO GAMEPAD",65,137,23,0xffa1ccdd,true,Paint.Align.LEFT);
        for(int i=0;i<ACTION_LABELS.length;i++) {
            int y=169+i*75;
            round(c,55,y,1225,y+63,12,0xff1b3354);
            text(c,ACTION_LABELS[i],81,y+43,28,WHITE,true,Paint.Align.LEFT);
            round(c,750,y+7,1205,y+56,9,remapAction==i?0xff0dbaac:0xff315a78);
            String name=KeyEvent.keyCodeToString(mapping[i]).replace("KEYCODE_", "").replace("BUTTON_","");
            centered(c,remapAction==i?"PRESSIONE UM BOTÃO...":name,977,y+40,23,WHITE,true);
        }
        round(c,55,644,440,701,12,0xff2ea8a6);
        centered(c,"VOLTAR",246,684,26,NAVY,true);
        round(c,478,644,906,701,12,0xff344965);
        centered(c,"RESTAURAR PADRÃO",689,684,26,WHITE,true);
        round(c,940,644,1225,701,12,audioOn?0xff3a8578:0xff514f64);
        centered(c,"SOM: "+(audioOn?"ON":"OFF"),1080,684,27,WHITE,true);
        if(remapAction>=0) {
            rectangle(c,0,0,W,H,0xb6071122);
            round(c,245,242,1035,479,24,0xff1b3959);
            centered(c,"MAPEANDO: "+ACTION_LABELS[remapAction],640,335,38,WHITE,true);
            centered(c,"PRESSIONE UM BOTÃO FÍSICO PARA ASSOCIAR",640,397,27,0xff8ce2eb,true);
            centered(c,"VOLTAR PARA CANCELAR",640,440,20,0xffc0cdd7,false);
        }
    }

    private void screenTouchUp(float x,float y) {
        if(screen==MENU) {
            if(y>=576 && y<=676) {
                if(x<449) {startRace(); return;}
                if(x<685) {lastSelection=MENU;screen=CONTROLS;return;}
            }
            if(y>=393 && y<=560) {
                if(x>=570 && x<687)trackIndex=(trackIndex+1)%TRACKS.length;
                else if(x>=687 && x<800)trackIndex=(trackIndex+TRACKS.length-1)%TRACKS.length;
            }
        } else if(screen==RACING) {
            if(x>1130 && y<220) screen=PAUSED;
        } else if(screen==PAUSED) {
            if(x>348 && x<934) {
                if(y>338 && y<441) screen=RACING;
                else if(y>450 && y<543) {screen=CONTROLS;lastSelection=PAUSED;}
                else if(y>548 && y<638) {screen=MENU;speed=0;}
            }
        } else if(screen==FINISHED) {
            if(x>=380 && x<=900 && y>=484 && y<=582) startRace();
            else screen=MENU;
        } else if(screen==CONTROLS) {
            if(remapAction>=0)return;
            if(y<635 && y>160 && x>=740) {
                int i=(int)((y-169)/75);
                if(i>=0 && i<ACTION_LABELS.length)remapAction=i;
            } else if(y>=642) {
                if(x<450) screen=lastSelection;
                else if(x<932) {
                    System.arraycopy(DEFAULT_KEYS,0,mapping,0,mapping.length);
                    SharedPreferences.Editor e=prefs.edit();
                    for(int i=0;i<mapping.length;i++)e.putInt("map_"+i,mapping[i]);
                    e.apply();
                } else {
                    audioOn=!audioOn;
                    prefs.edit().putBoolean("audio_on",audioOn).apply();
                }
            }
        }
    }
    @Override public boolean onTouchEvent(MotionEvent ev) {
        synchronized(stateLock) {
            int action=ev.getActionMasked();
            if(action==MotionEvent.ACTION_DOWN || action==MotionEvent.ACTION_POINTER_DOWN) {
                int at=ev.getActionIndex();int id=ev.getPointerId(at);
                touches.add(new TouchPoint(id,ev.getX(at)*W/Math.max(1f,getWidth()),ev.getY(at)*H/Math.max(1f,getHeight())));
            } else if(action==MotionEvent.ACTION_MOVE) {
                for(TouchPoint t:touches) {
                    for(int i=0;i<ev.getPointerCount();i++)if(ev.getPointerId(i)==t.id) {
                        t.x=ev.getX(i)*W/Math.max(1f,getWidth());t.y=ev.getY(i)*H/Math.max(1f,getHeight());
                    }
                }
            } else if(action==MotionEvent.ACTION_UP || action==MotionEvent.ACTION_POINTER_UP) {
                int at=ev.getActionIndex();int id=ev.getPointerId(at);
                float x=ev.getX(at)*W/Math.max(1f,getWidth());
                float y=ev.getY(at)*H/Math.max(1f,getHeight());
                touches.removeIf(t->t.id==id);
                screenTouchUp(x,y);
            } else if(action==MotionEvent.ACTION_CANCEL) touches.clear();
            return true;
        }
    }
    private boolean isGamepad(int source) {
        return ((source&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD)
                || ((source&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK)
                || ((source&InputDevice.SOURCE_DPAD)==InputDevice.SOURCE_DPAD);
    }
    public boolean handleKeyEvent(KeyEvent event) {
        synchronized(stateLock) {
            int key=event.getKeyCode();
            if(screen==CONTROLS && remapAction>=0) {
                if(event.getAction()==KeyEvent.ACTION_DOWN && event.getRepeatCount()==0) {
                    if(key==KeyEvent.KEYCODE_BACK) remapAction=-1;
                    else if(isGamepad(event.getSource())) {
                        mapping[remapAction]=key;
                        prefs.edit().putInt("map_"+remapAction,key).apply();
                        remapAction=-1;
                        tone(ToneGenerator.TONE_PROP_ACK,130);
                    }
                }
                return true;
            }
            if(key==KeyEvent.KEYCODE_BACK && event.getAction()==KeyEvent.ACTION_DOWN) {
                if(screen==CONTROLS)screen=lastSelection;
                else togglePause();
                return true;
            }
            if(screen==MENU && event.getAction()==KeyEvent.ACTION_DOWN &&
                    (key==KeyEvent.KEYCODE_BUTTON_START || key==KeyEvent.KEYCODE_ENTER)) {
                startRace();return true;
            }
            if(screen==RACING && key==mapping[5] && event.getAction()==KeyEvent.ACTION_DOWN) {
                screen=PAUSED;return true;
            }
            if(screen==PAUSED && key==mapping[5] && event.getAction()==KeyEvent.ACTION_DOWN) {
                screen=RACING;return true;
            }
            if(screen==RACING || screen==PAUSED) {
                if(event.getAction()==KeyEvent.ACTION_DOWN)pressedKeys.add(key);
                else if(event.getAction()==KeyEvent.ACTION_UP)pressedKeys.remove(key);
                return isGamepad(event.getSource());
            }
            return false;
        }
    }
    public boolean handleMotion(MotionEvent event) {
        if(!isGamepad(event.getSource()))return false;
        synchronized(stateLock) {
            axisSteering=event.getAxisValue(MotionEvent.AXIS_X);
            float hat=event.getAxisValue(MotionEvent.AXIS_HAT_X);
            if(Math.abs(hat)>0.4)axisSteering=hat;
            if(Math.abs(axisSteering)<0.12)axisSteering=0;
            axisGas=Math.max(event.getAxisValue(MotionEvent.AXIS_RTRIGGER),event.getAxisValue(MotionEvent.AXIS_GAS));
            axisBrake=Math.max(event.getAxisValue(MotionEvent.AXIS_LTRIGGER),event.getAxisValue(MotionEvent.AXIS_BRAKE));
        }
        return true;
    }
    public void togglePause() {
        synchronized(stateLock) {
            if(screen==RACING)screen=PAUSED;
            else if(screen==PAUSED)screen=RACING;
            else if(screen==CONTROLS) {remapAction=-1;screen=lastSelection;}
        }
    }
}
