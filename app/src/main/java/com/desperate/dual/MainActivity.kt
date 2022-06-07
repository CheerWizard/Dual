package com.desperate.dual

import android.Manifest
import android.os.Bundle
import android.util.Log
import android.view.*
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.desperate.dual.databinding.ActivityMainBinding
import com.google.android.material.dialog.MaterialAlertDialogBuilder

class MainActivity : AppCompatActivity() {

    companion object {
        private val TAG = MainActivity::class.simpleName

        init {
            System.loadLibrary("dual")
        }
    }

    private lateinit var binding: ActivityMainBinding

    private val cameraListAdapter = CameraListAdapter(object : CameraListItemListener {
        override fun onClicked(camera: Camera) {
            NativeCamera.close()
            NativeCamera.stopPreview()

            val isOpened = NativeCamera.open(camera.id)
            val message = if (isOpened) "opened!" else "failed to open!"
            Toast.makeText(
                this@MainActivity,
                "Camera has been $message",
                Toast.LENGTH_SHORT
            ).show()

            if (surfaceReady) {
                NativeCamera.startPreview(binding.cameraSurfaceView.holder.surface)
                Log.w(TAG, "Starting preview of $camera")
            } else {
                Log.w(TAG, "Unable to start preview of $camera")
            }
        }
    })

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            permissions.entries.forEach {
                if (it.value) {
                    Log.w(TAG, "${it.key} permission granted!")
                } else {
                    Log.w(TAG, "${it.key} permission denied!")
                    showErrorDialog(title, "${it.key} permission denied! Please grant this permission to continue!")
                }
            }
        }

    private var surfaceReady = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        Dual.init()
        requestPermissionLauncher.launch(arrayOf(Manifest.permission.CAMERA))
        initView()
        onRefreshBtnPressed()
    }

    private fun initView() {
        binding.apply {
            refresh.setOnClickListener { onRefreshBtnPressed() }

            cameraListView.layoutManager = LinearLayoutManager(
                applicationContext,
                LinearLayoutManager.HORIZONTAL,
                false
            )
            cameraListView.adapter = cameraListAdapter

            cameraSurfaceView.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    Log.v(TAG, "surfaceCreated()")
                    surfaceReady = true
                    if (cameraListAdapter.isNotEmpty()) {
                        NativeCamera.open(cameraListAdapter.first().id)
                        NativeCamera.startPreview(holder.surface)
                    }
                }

                override fun surfaceDestroyed(holder: SurfaceHolder) {
                    Log.v(TAG, "surfaceDestroyed()")
                    surfaceReady = false
                    NativeCamera.close()
                    NativeCamera.stopPreview()
                }

                override fun surfaceChanged(
                    holder: SurfaceHolder,
                    format: Int,
                    width: Int,
                    height: Int
                ) {
                    Log.v(TAG, "surfaceChanged(format=$format w=$width, h=$height)")
                }
            })
        }
    }

    private fun onRefreshBtnPressed() {
        cameraListAdapter.replaceAll(NativeCamera.getAvailableCameras())
    }

    private fun showErrorDialog(title: CharSequence, message: String) {
        MaterialAlertDialogBuilder(this).apply {
            setTitle(title)
            setMessage(message)
            setPositiveButton(android.R.string.ok) { dialog, _ ->
                dialog.dismiss()
                finish()
            }
            create()
            show()
            setCancelable(false)
            setFinishOnTouchOutside(false)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        Dual.release()
    }
}