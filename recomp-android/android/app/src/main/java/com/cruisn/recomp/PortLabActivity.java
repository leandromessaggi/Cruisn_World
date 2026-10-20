package com.cruisn.recomp;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Verification workbench for the actual ROM, NOT a playable game yet. */
public class PortLabActivity extends Activity {
    static { System.loadLibrary("cw_boot"); }
    private static final int PICK_ROM=123;
    private static final String EXPECTED_SHA="5320594232f0c492a672464832425351bd06497c95fb243745e2493bc5df9c2c";
    private TextView log;
    private static native String nativeBootProbe(String romPath);

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().getDecorView().setSystemUiVisibility(5894 | 1024 | 512);
        LinearLayout root=new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL); root.setPadding(32,18,32,18);
        root.setBackgroundColor(Color.rgb(9,18,31));
        TextView title=new TextView(this);title.setText("CRUIS'N WORLD • RECOMPILAÇÃO ANDROID");
        title.setTextColor(0xfff3ad56);title.setTextSize(22);
        title.setGravity(Gravity.CENTER_HORIZONTAL);root.addView(title);
        TextView explain=new TextView(this);
        explain.setText("Etapa 3: ROM validada, mapa de 8 MiB, overlay real e 17 funções MIPS recompiladas em C++ ARM64. Ainda NÃO executa a corrida.");
        explain.setTextColor(Color.WHITE);explain.setTextSize(15);root.addView(explain);
        Button pick=new Button(this);pick.setText("SELECIONAR ROM .N64 / .Z64 / .V64 / ZIP");
        root.addView(pick);pick.setOnClickListener(v->{
            Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);i.setType("*/*");
            i.addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(i,PICK_ROM);
        });
        ScrollView scroll=new ScrollView(this);
        log=new TextView(this);log.setTextColor(0xff7be0d6);log.setTextSize(15);
        log.setTypeface(android.graphics.Typeface.MONOSPACE);
        log.setText("Aguardando ROM...\n\nSem código de emulação ou pistas inventadas neste aplicativo.");
        scroll.addView(log);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        setContentView(root);
        File f=new File(getFilesDir(),"cruisn_verified.z64");
        if(f.exists())new Thread(()->verifyFile(f)).start();
    }

    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request!=PICK_ROM||result!=RESULT_OK||data==null)return;
        Uri uri=data.getData(); if(uri==null)return;
        log.setText("Lendo e validando ROM...");
        new Thread(()->{
            try {
                byte[] raw=readRom(uri);
                byte[] canonical=normalize(raw);
                if(canonical.length!=12582912L)throw new Exception("Tamanho da ROM incompatível: "+canonical.length);
                String hash=sha256(canonical);
                if(!hash.equals(EXPECTED_SHA))throw new Exception("SHA-256 não corresponde à versão americana suportada: "+hash);
                File f=new File(getFilesDir(),"cruisn_verified.z64");
                try(FileOutputStream os=new FileOutputStream(f)){os.write(canonical);}
                verifyFile(f);
            } catch(Exception e){runOnUiThread(()->log.setText("Falha de validação: "+e.getMessage()));}
        }).start();
    }
    private byte[] readRom(Uri uri) throws Exception {
        try(InputStream in=getContentResolver().openInputStream(uri)) {
            if(in==null)throw new Exception("Não foi possível abrir arquivo");
            byte[] original=readAll(in,128*1024*1024);
            if(original.length>=2&&original[0]=='P'&&original[1]=='K'){
                try(ZipInputStream zis=new ZipInputStream(new java.io.ByteArrayInputStream(original))){
                    ZipEntry ent;
                    while((ent=zis.getNextEntry())!=null){
                        String name=ent.getName().toLowerCase();
                        if(!ent.isDirectory()&&(name.endsWith(".n64")||name.endsWith(".v64")||name.endsWith(".z64")))return readAll(zis,64*1024*1024);
                    }
                }
                throw new Exception("ZIP sem ROM N64");
            }
            return original;
        }
    }
    private static byte[] readAll(InputStream in,int max) throws Exception {
        ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] buf=new byte[65536];int n;
        while((n=in.read(buf))>=0){if(n==0)continue;if(out.size()>max-n)throw new Exception("Arquivo grande demais");out.write(buf,0,n);}
        return out.toByteArray();
    }
    private static byte[] normalize(byte[] b) throws Exception {
        if(b.length<64)throw new Exception("ROM incompleta");
        if((b[0]&255)==0x80&&(b[1]&255)==0x37&&(b[2]&255)==0x12&&(b[3]&255)==0x40)return b;
        if((b[0]&255)==0x37&&(b[1]&255)==0x80&&(b[2]&255)==0x40&&(b[3]&255)==0x12){
            for(int i=0;i+1<b.length;i+=2){byte x=b[i];b[i]=b[i+1];b[i+1]=x;}return b;
        }
        if((b[0]&255)==0x40&&(b[1]&255)==0x12&&(b[2]&255)==0x37&&(b[3]&255)==0x80){
            for(int i=0;i+3<b.length;i+=4){byte a=b[i],c=b[i+1];b[i]=b[i+3];b[i+1]=b[i+2];b[i+2]=c;b[i+3]=a;}return b;
        }
        throw new Exception("Formato de ROM N64 não reconhecido");
    }
    private static String sha256(byte[] b)throws Exception{
        byte[] hash=MessageDigest.getInstance("SHA-256").digest(b);
        StringBuilder out=new StringBuilder();for(byte x:hash)out.append(String.format(java.util.Locale.ROOT,"%02x",x&255));return out.toString();
    }
    private void verifyFile(File f){
        String probe=nativeBootProbe(f.getAbsolutePath());
        runOnUiThread(()->log.setText(probe));
    }
}
