package com.desperate.dual

import android.util.Log
import android.view.LayoutInflater
import android.view.Surface
import android.view.SurfaceHolder
import android.view.ViewGroup
import android.widget.ImageButton
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import com.desperate.dual.databinding.CameraViewBinding


interface CameraListItemListener {
    fun onClicked(camera: Camera)
}

class CameraListAdapter(
    private val listItemListener: CameraListItemListener,
    private val list: ArrayList<Camera> = arrayListOf(),
) : RecyclerView.Adapter<CameraListAdapter.CameraViewHolder>() {

    companion object {
        private val TAG = CameraListAdapter::class.simpleName
    }

    inner class CameraViewHolder(val binding: CameraViewBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CameraViewHolder {
        val itemView = CameraViewBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return CameraViewHolder(itemView)
    }

    override fun onBindViewHolder(holder: CameraViewHolder, position: Int) {
        val camera = list[position]
        holder.binding.apply {
            Log.d(CameraListAdapter::class.simpleName,"Camera id: $camera")
            lensName.text = camera.lensName
            root.setOnClickListener { listItemListener.onClicked(camera) }
            surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(holder: SurfaceHolder) {
                    Log.v(TAG, "surfaceCreated()")
                    updatePreview(camera, holder.surface, disablePreviewIcon)
                }

                override fun surfaceDestroyed(holder: SurfaceHolder) {
                    Log.v(TAG, "surfaceDestroyed()")
                    disablePreview(camera, disablePreviewIcon)
                }

                override fun surfaceChanged(
                    holder: SurfaceHolder,
                    format: Int,
                    width: Int,
                    height: Int
                ) {
                    Log.v(TAG, "surfaceChanged(cameraId=${camera.id} format=$format w=$width, h=$height)")
                }
            })

            previewIcon.setOnClickListener {
                list[position].apply { isEnabled = !isEnabled }
                disablePreview(camera, disablePreviewIcon)
            }

            disablePreviewIcon.setOnClickListener {
                list[position].apply { isEnabled = !isEnabled }
                updatePreview(camera, surfaceView.holder.surface, disablePreviewIcon)
            }

            updatePreview(camera, surfaceView.holder.surface, disablePreviewIcon)
        }
    }

    private fun disablePreview(camera: Camera, disableIcon: ImageButton) {
        if (!camera.isEnabled) {
            NativeCamera.close(camera.id)
            NativeCamera.stopPreview(camera.id)
            disableIcon.isVisible = true
            disableIcon.isEnabled = true
        }
    }

    private fun updatePreview(camera: Camera, surface: Surface, disableIcon: ImageButton) {
        disablePreview(camera, disableIcon)
        if (camera.isEnabled) {
            NativeCamera.open(camera.id)
            NativeCamera.startPreview(surface, camera.id)
            disableIcon.isVisible = false
            disableIcon.isEnabled = false
        }
    }

    override fun getItemCount(): Int = list.size

    fun replaceAll(list: List<Camera>) {
        this.list.clear()
        this.list.addAll(list)
        notifyDataSetChanged()
    }

}