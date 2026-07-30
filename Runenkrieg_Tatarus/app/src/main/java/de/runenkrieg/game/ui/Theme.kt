package de.runenkrieg.game.ui

import android.app.Activity
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

private val RuneColors = darkColorScheme(
    primary = Color(0xFF67E8F9),
    onPrimary = Color(0xFF082F49),
    secondary = Color(0xFFC4B5FD),
    tertiary = Color(0xFFFCD34D),
    background = Color(0xFF090D18),
    onBackground = Color(0xFFE6EDF7),
    surface = Color(0xFF131B2B),
    onSurface = Color(0xFFE6EDF7),
    surfaceVariant = Color(0xFF202C42),
    onSurfaceVariant = Color(0xFFC5D1E6),
    error = Color(0xFFFF8A80)
)

@Composable
fun RunenkriegTheme(content: @Composable () -> Unit) {
    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            window.statusBarColor = Color.Transparent.toArgb()
            window.navigationBarColor = Color(0xFF090D18).toArgb()
            WindowCompat.getInsetsController(window, view).isAppearanceLightStatusBars = false
            WindowCompat.getInsetsController(window, view).isAppearanceLightNavigationBars = false
        }
    }
    MaterialTheme(colorScheme = RuneColors, content = content)
}
