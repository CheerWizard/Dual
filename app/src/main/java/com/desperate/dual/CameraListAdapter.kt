package com.desperate.dual

import android.util.Log
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.desperate.dual.databinding.CameraViewBinding

interface CameraListItemListener {
    fun onClicked(camera: Camera)
}

class CameraListAdapter(
    private val listItemListener: CameraListItemListener,
    private val list: ArrayList<Camera> = arrayListOf(),
) : RecyclerView.Adapter<CameraListAdapter.CameraViewHolder>() {

    inner class CameraViewHolder(
        val binding: CameraViewBinding
    ) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CameraViewHolder {
        val itemView = CameraViewBinding.inflate(LayoutInflater.from(parent.context), parent,false)
        Log.d(CameraListAdapter::class.simpleName,"Camera list size: ${list.size}")
        return CameraViewHolder(itemView)
    }

    override fun onBindViewHolder(holder: CameraViewHolder, position: Int) {
        val camera = list[position]
        holder.binding.apply {
            Log.d(CameraListAdapter::class.simpleName,"Camera id: $camera")
            lensName.text = camera.lensName
            root.setOnClickListener { listItemListener.onClicked(camera) }
        }
    }

    override fun getItemCount(): Int = list.size

    fun replaceAll(cameraList: List<Camera>) {
        this.list.clear()
        this.list.addAll(cameraList)
        notifyDataSetChanged()
    }

    fun isNotEmpty(): Boolean = list.isNotEmpty()

    fun first(): Camera = list.first()
}