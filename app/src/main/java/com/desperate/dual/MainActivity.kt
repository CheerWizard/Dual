package com.desperate.dual

import android.Manifest
import android.os.Bundle
import android.util.Log
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

    private val cameraListAdapter =  CameraListAdapter(object : CameraListItemListener {
        override fun onClicked(camera: Camera) {
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
            cameraListView.adapter = cameraListAdapter
            cameraListView.layoutManager = LinearLayoutManager(applicationContext, LinearLayoutManager.VERTICAL, false)
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