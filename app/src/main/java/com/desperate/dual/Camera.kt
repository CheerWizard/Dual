package com.desperate.dual

data class Camera(
    val id: String,
    val lensId: Int,
    val lensName: String,
    var isEnabled: Boolean = false
) {
    override fun toString(): String = "Camera: id=$id lensId=$lensName lensName=$lensName isEnabled=$isEnabled"
}