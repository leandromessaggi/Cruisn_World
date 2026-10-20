package com.draco.ludere.input

import android.app.Activity
import androidx.appcompat.app.AlertDialog
import android.content.Context
import android.view.InputDevice
import android.view.KeyEvent
import android.widget.Toast

/** Maps a physical Android gamepad button to the LibretroDroid RetroPad key. */
class ControllerMapping(private val activity: Activity) {
    private val prefs = activity.getSharedPreferences("cw64_controls", Context.MODE_PRIVATE)
    private data class Target(val label: String, val key: Int)
    private val targets = listOf(
        Target("A", KeyEvent.KEYCODE_BUTTON_A),
        Target("B", KeyEvent.KEYCODE_BUTTON_B),
        Target("C / X", KeyEvent.KEYCODE_BUTTON_X),
        Target("C / Y", KeyEvent.KEYCODE_BUTTON_Y),
        Target("L", KeyEvent.KEYCODE_BUTTON_L1),
        Target("R", KeyEvent.KEYCODE_BUTTON_R1),
        Target("Z / L2", KeyEvent.KEYCODE_BUTTON_L2),
        Target("R2", KeyEvent.KEYCODE_BUTTON_R2),
        Target("START", KeyEvent.KEYCODE_BUTTON_START),
        Target("SELECT", KeyEvent.KEYCODE_BUTTON_SELECT),
        Target("Direcional cima", KeyEvent.KEYCODE_DPAD_UP),
        Target("Direcional baixo", KeyEvent.KEYCODE_DPAD_DOWN),
        Target("Direcional esquerda", KeyEvent.KEYCODE_DPAD_LEFT),
        Target("Direcional direita", KeyEvent.KEYCODE_DPAD_RIGHT)
    )

    private fun isController(event: KeyEvent): Boolean =
        event.source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
        event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK ||
        event.source and InputDevice.SOURCE_DPAD == InputDevice.SOURCE_DPAD

    fun remap(key: Int, event: KeyEvent): Int =
        if (isController(event)) prefs.getInt("key_$key", key) else key

    fun forceTouch(): Boolean = prefs.getBoolean("touch_always", false)

    fun toggleTouch(): Boolean {
        val on = !forceTouch()
        prefs.edit().putBoolean("touch_always",on).apply()
        return on
    }

    fun showRemapDialog() {
        val entries = targets.map { it.label }.toMutableList()
        entries.add("Restaurar mapeamento")
        AlertDialog.Builder(activity)
            .setTitle("Mapear controle fisico")
            .setItems(entries.toTypedArray()) { _, which ->
                if (which == targets.size) {
                    prefs.edit().clear().apply()
                    Toast.makeText(activity,"Mapeamento restaurado",Toast.LENGTH_SHORT).show()
                } else capture(targets[which])
            }
            .setNegativeButton("Fechar",null).show()
    }

    private fun capture(target: Target) {
        val dialog = AlertDialog.Builder(activity)
            .setTitle("Mapear " + target.label)
            .setMessage("Pressione o botao desejado no controle. Voltar cancela.")
            .setNegativeButton("Cancelar",null).create()
        dialog.setOnKeyListener { _, code, event ->
            if (event.action != KeyEvent.ACTION_DOWN) return@setOnKeyListener false
            if (code == KeyEvent.KEYCODE_BACK) {
                dialog.dismiss()
                return@setOnKeyListener true
            }
            if (!isController(event)) return@setOnKeyListener false
            prefs.edit().putInt("key_$code",target.key).apply()
            Toast.makeText(activity,KeyEvent.keyCodeToString(code)+" → "+target.label,
                Toast.LENGTH_LONG).show()
            dialog.dismiss()
            true
        }
        dialog.show()
    }
}
