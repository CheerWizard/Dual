package com.desperate.dual

import android.view.Surface

object NativeCamera {

    fun getAvailableCameras(): List<Camera> = getIdList().map {
        val lensId = getLensId(it)
        Camera(it, lensId, getLensName(lensId))
    }

    external fun getIdList(): Array<String>
    external fun getLensId(cameraId: String): Int
    external fun getLensName(lensId: Int): String

    external fun open(cameraId: String): Boolean
    external fun close(): Boolean

    external fun startPreview(surface: Surface)
    external fun stopPreview()

}