#!/usr/bin/env python3
"""Generate a standalone Android N64 player from pinned GPL-3 Ludere sources.
Public builds request the user's own ROM at first launch.
Do not publish commercial game ROM files.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parent
BASE="https://github.com/tytydraco/Ludere.git"
REV="8ec1ca8bea0c49ab372792cbb746c91e832c8c9e"
ROM=ROOT/"game_rom"/"CruisnWorld.n64"

def patch(file,old,new):
    file=Path(file)
    text=file.read_text(encoding="utf-8")
    n=text.count(old)
    if n!=1:
        raise RuntimeError("Expected exactly one match in %s; got %d: %s" % (file,n,old[:80]))
    file.write_text(text.replace(old,new),encoding="utf-8")

def gradle(project):
    patch(project/"build.gradle",'ext.kotlin_version = "1.5.31"',
          'ext.kotlin_version = "1.7.10"')
    patch(project/"build.gradle","com.android.tools.build:gradle:7.2.0-alpha05",
          "com.android.tools.build:gradle:7.4.2")
    patch(project/"gradle/wrapper/gradle-wrapper.properties",
          "gradle-7.3-rc-1-bin.zip","gradle-7.5.1-bin.zip")
    app=project/"app/build.gradle"
    for old,new in [
        ("compileSdkVersion 31","compileSdkVersion 33"),
        ("buildToolsVersion '30.0.3'","buildToolsVersion '33.0.2'"),
        ("targetSdkVersion 31","targetSdkVersion 33"),
        ("include 'x86', 'x86_64', 'armeabi-v7a', 'arm64-v8a'",
         "include 'arm64-v8a'"),
        ("['x86', 'x86_64', 'armeabi-v7a', 'arm64-v8a']",
         "['arm64-v8a']"),
        ("    splits {\n        abi {",
         "    packagingOptions {\n        jniLibs { useLegacyPackaging true }\n    }\n\n    splits {\n        abi {")
    ]: patch(app,old,new)

def configure(project):
    p=project/"app/src/main/res/values/config.xml"
    tree=ET.parse(p)
    opts=dict(config_id="cruisnworld64", config_name="Cruis'n World",
      config_core="mupen64plus_next_gles3",config_load_bytes="false",
      config_variables="",config_fullscreen="true",config_orientation="2",
      config_gamepad="true",config_left_analog="true",
      config_gamepad_a="true",config_gamepad_b="true",config_gamepad_x="true",
      config_gamepad_y="true",config_gamepad_l1="true",config_gamepad_r1="true",
      config_gamepad_start="true",config_gamepad_select="true")
    found=set()
    for node in tree.getroot():
        name=node.attrib.get("name")
        if name in opts:
            node.text=opts[name]
            found.add(name)
    assert found==set(opts),set(opts)-found
    tree.write(p,encoding="utf-8",xml_declaration=True)
    patch(p,"</resources>",
      '<bool name="config_rom_bundled">'+str(ROM.exists()).lower()+'</bool>\n</resources>')
    p=project/"app/src/main/res/values/strings.xml"
    patch(p,"</resources>",'''<string name="menu_controller_mapping">Mapear botoes fisicos</string>
    <string name="menu_touch_always">Alternar touch com controle conectado</string>
</resources>''')

def modify_gamepad(project):
    p=project/"app/src/main/java/com/draco/ludere/gamepad/GamePadConfig.kt"
    patch(p,"        val BUTTON_L1 = ButtonConfig(",
      '''        val BUTTON_L2 = ButtonConfig(
            id = KeyEvent.KEYCODE_BUTTON_L2,
            label = "Z"
        )
        val BUTTON_L1 = ButtonConfig(''')
    patch(p,'SecondaryDialConfig.SingleButton(4, 1, BUTTON_L1).takeIf { resources.getBoolean(R.bool.config_gamepad_l1) },',
      '''SecondaryDialConfig.SingleButton(4, 1, BUTTON_L1).takeIf { resources.getBoolean(R.bool.config_gamepad_l1) },
            SecondaryDialConfig.SingleButton(6, 1, BUTTON_L2),''')
    patch(p,'label = "X"','label = "C↑"')
    patch(p,'label = "Y"','label = "C↓"')

def modify_vm(project):
    p=project/"app/src/main/java/com/draco/ludere/viewmodels/GameActivityViewModel.kt"
    patch(p,"    private val controllerInput = ControllerInput()",
      '''    private val controllerInput = ControllerInput()
    var mappingCallback: () -> Unit = {}
    var forceTouchProvider: () -> Boolean = { false }
    var toggleTouchCallback: () -> Unit = {}''')
    patch(p,"val visibility = if (GamePad.shouldShowGamePads(activity))",
          "val visibility = if (forceTouchProvider() || GamePad.shouldShowGamePads(activity))")
    patch(p,"context.getString(R.string.menu_fast_forward)\n",
      """context.getString(R.string.menu_fast_forward),
            context.getString(R.string.menu_controller_mapping),
            context.getString(R.string.menu_touch_always)
""")
    patch(p,'''                context.getString(R.string.menu_fast_forward) -> retroView?.let {
                    retroViewUtils?.fastForward(it)
                }''',
      '''                context.getString(R.string.menu_fast_forward) -> retroView?.let {
                    retroViewUtils?.fastForward(it)
                }
                context.getString(R.string.menu_controller_mapping) -> mappingCallback()
                context.getString(R.string.menu_touch_always) -> toggleTouchCallback()''')

def modify_activity(project):
    p=project/"app/src/main/java/com/draco/ludere/views/GameActivity.kt"
    patch(p,"import android.view.MotionEvent",
      '''import android.view.MotionEvent
import android.net.Uri
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import com.draco.ludere.input.ControllerMapping
import com.draco.ludere.repositories.Storage
import java.io.PushbackInputStream
import java.util.zip.ZipInputStream''')
    patch(p,"    private val viewModel: GameActivityViewModel by viewModels()",
      '''    private val viewModel: GameActivityViewModel by viewModels()
    private val controllerMapping by lazy { ControllerMapping(this) }
    private var gameStarted = false
    private val romPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) showRomSelector() else try {
            importRom(uri)
            startGame()
        } catch (e: Exception) {
            Storage.getInstance(this).rom.delete()
            Toast.makeText(this, "ROM invalida: \${e.message}", Toast.LENGTH_LONG).show()
            showRomSelector()
        }
    }''')
    patch(p,'''        registerInputListener()
        viewModel.setConfigOrientation(this)
        viewModel.updateGamePadVisibility(this, binding.leftContainer, binding.rightContainer)
        viewModel.prepareMenu(this)
        viewModel.setupRetroView(this, binding.retroviewContainer)
        viewModel.setupGamePads(binding.leftContainer, binding.rightContainer)''',
      '''        viewModel.mappingCallback = { controllerMapping.showRemapDialog() }
        viewModel.forceTouchProvider = { controllerMapping.forceTouch() }
        viewModel.toggleTouchCallback = {
            controllerMapping.toggleTouch()
            viewModel.updateGamePadVisibility(this, binding.leftContainer, binding.rightContainer)
        }
        viewModel.setConfigOrientation(this)
        val rom = Storage.getInstance(this).rom
        if (resources.getBoolean(com.draco.ludere.R.bool.config_rom_bundled) ||
            (rom.exists() && rom.length() > 1024 * 1024)) startGame()
        else showRomSelector()''')
    patch(p,"viewModel.processKeyEvent(keyCode, event) ?: super.onKeyDown(keyCode, event)",
      "viewModel.processKeyEvent(controllerMapping.remap(keyCode, event), event) ?: super.onKeyDown(keyCode, event)")
    patch(p,"viewModel.processKeyEvent(keyCode, event) ?: super.onKeyUp(keyCode, event)",
      "viewModel.processKeyEvent(controllerMapping.remap(keyCode, event), event) ?: super.onKeyUp(keyCode, event)")
    patch(p,"    override fun onBackPressed() = viewModel.showMenu()",
      '''    private fun startGame() {
        if (gameStarted) return
        gameStarted = true
        registerInputListener()
        viewModel.updateGamePadVisibility(this, binding.leftContainer, binding.rightContainer)
        viewModel.prepareMenu(this)
        viewModel.setupRetroView(this, binding.retroviewContainer)
        viewModel.setupGamePads(binding.leftContainer, binding.rightContainer)
    }

    private fun showRomSelector() {
        AlertDialog.Builder(this)
            .setTitle("Cruis'n World")
            .setMessage("Selecione sua ROM de Nintendo 64 (.n64, .v64, .z64 ou .zip). Ela sera copiada para o armazenamento privado do aplicativo.")
            .setPositiveButton("Selecionar ROM") { _, _ -> romPicker.launch(arrayOf("*/*")) }
            .setNegativeButton("Sair") { _, _ -> finish() }
            .setCancelable(false)
            .show()
    }

    private fun importRom(uri: Uri) {
        val dest = Storage.getInstance(this).rom
        dest.parentFile?.mkdirs()
        val raw = contentResolver.openInputStream(uri) ?: error("Falha ao abrir o arquivo")
        raw.use { stream ->
            val input = PushbackInputStream(stream, 4)
            val header = ByteArray(4)
            val n = input.read(header)
            if (n > 0) input.unread(header, 0, n)
            if (n >= 2 && header[0] == 80.toByte() && header[1] == 75.toByte()) {
                ZipInputStream(input).use { zip ->
                    var entry = zip.nextEntry
                    var found = false
                    while (entry != null) {
                        val lower = entry.name.lowercase()
                        if (!entry.isDirectory &&
                            listOf(".n64", ".v64", ".z64").any { lower.endsWith(it) }) {
                            dest.outputStream().use { zip.copyTo(it) }
                            found = true
                            break
                        }
                        entry = zip.nextEntry
                    }
                    if (!found) error("ZIP sem ROM Nintendo 64")
                }
            } else {
                dest.outputStream().use { input.copyTo(it) }
            }
        }
        if (dest.length() < 1024 * 1024 || dest.length() > 128L * 1024 * 1024)
            error("Tamanho de ROM inesperado")
        val signature = ByteArray(4)
        dest.inputStream().use { if (it.read(signature) != 4) error("ROM incompleta") }
        val hex = signature.joinToString("") { (it.toInt() and 255).toString(16).padStart(2,'0') }
        if (hex !in listOf("80371240","37804012","40123780"))
            error("Cabeçalho nao e de Nintendo 64")
    }

    override fun onBackPressed() {
        if (gameStarted) viewModel.showMenu() else showRomSelector()
    }''')

def assets(project):
    import cairosvg
    art=(ROOT/"assets"/"launcher_icon.svg").read_bytes()
    drawable=project/"app/src/main/res/drawable"
    drawable.mkdir(parents=True,exist_ok=True)
    cairosvg.svg2png(bytestring=art,write_to=str(drawable/"launcher_foreground.png"),
                    output_width=432,output_height=432)
    for folder,px in [("mdpi",48),("hdpi",72),("xhdpi",96),("xxhdpi",144),("xxxhdpi",192)]:
        target=project/"app/src/main/res"/("mipmap-"+folder)
        target.mkdir(parents=True,exist_ok=True)
        for name in ("ic_launcher.png","ic_launcher_round.png"):
            cairosvg.svg2png(bytestring=art,write_to=str(target/name),
                            output_width=px,output_height=px)
    drawable_xml='''<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
<background android:drawable="@color/cw_icon_background"/>
<foreground android:drawable="@drawable/launcher_foreground"/>
</adaptive-icon>
'''
    folder=project/"app/src/main/res/mipmap-anydpi-v26"
    folder.mkdir(parents=True,exist_ok=True)
    for n in ("ic_launcher.xml","ic_launcher_round.xml"):
        (folder/n).write_text(drawable_xml,encoding="utf-8")
    p=project/"app/src/main/res/values/colors.xml"
    patch(p,"</resources>",
      '<color name="cw_icon_background">#08152B</color>\n</resources>')

def prepare(dest,refresh):
    if ROM.exists():
        sig=ROM.open("rb").read(4)
        if ROM.stat().st_size < 1048576 or sig not in (
            bytes.fromhex("80371240"),bytes.fromhex("37804012"),bytes.fromhex("40123780")):
            raise RuntimeError("game_rom/CruisnWorld.n64 is not a valid ROM")
    if refresh and dest.exists():
        shutil.rmtree(dest)
    if not dest.exists():
        subprocess.run(["git","clone",BASE,str(dest)],check=True)
        subprocess.run(["git","-C",str(dest),"checkout",REV],check=True)
    if (dest/".cruisnworld_prepared").exists():
        return
    gradle(dest)
    configure(dest)
    modify_gamepad(dest)
    modify_vm(dest)
    modify_activity(dest)
    mapping=dest/"app/src/main/java/com/draco/ludere/input/ControllerMapping.kt"
    shutil.copy2(ROOT/"overlay/ControllerMapping.kt",mapping)
    raw=dest/"app/src/main/res/raw"
    raw.mkdir(parents=True,exist_ok=True)
    if ROM.exists():
        shutil.copy2(ROM,raw/"rom.n64")
    else:
        (raw/"rom.n64").write_bytes(b"Import your own N64 ROM at first launch")
    assets(dest)
    manifest=dest/"app/src/main/AndroidManifest.xml"
    patch(manifest,'android:allowBackup="true"','android:allowBackup="false"')
    (dest/".cruisnworld_prepared").write_text(REV+"\n")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--prepare-only",action="store_true")
    ap.add_argument("--refresh",action="store_true")
    ap.add_argument("--output",default="android-project")
    args=ap.parse_args()
    dest=(ROOT/args.output).resolve()
    prepare(dest,args.refresh)
    if args.prepare_only:
        print("Android project prepared:",dest)
        return
    wrapper=dest/"gradlew"
    wrapper.chmod(wrapper.stat().st_mode|0o111)
    subprocess.run([str(wrapper),"--no-daemon","assembleRelease"],cwd=dest,check=True)
    apks=list((dest/"app/build/outputs/apk/release").glob("*.apk"))
    if not apks: raise RuntimeError("No APK produced by Gradle")
    chosen=next((x for x in apks if "universal" in x.name),apks[0])
    shutil.copy2(chosen,ROOT/"CruisnWorld-arm64.apk")
    print("APK:",ROOT/"CruisnWorld-arm64.apk")

if __name__=="__main__":
    try: main()
    except (RuntimeError,subprocess.CalledProcessError,FileNotFoundError) as e:
        print("BUILD ERROR:",e,file=sys.stderr)
        sys.exit(1)
