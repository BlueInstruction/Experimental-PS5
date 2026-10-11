package com.px5.emulator.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.px5.emulator.R
import com.px5.emulator.core.Px5Settings

// ---------------------------------------------------------------------------
// PX5 palette — AMOLED-first identity (v1.61).
//
// Visual direction: premium console dark, tuned for OLED phone panels.
//   * Dark mode is TRUE BLACK (#000000) — AMOLED pixels off, no misty
//     gray veils, no washed-out elevation fog. Depth comes from hairlines
//     and surface steps, not from brightening the background.
//   * Light mode is a CLEAN neutral gray — no foggy blue-white, no
//     translucent haze; crisp contrast between surfaces and text.
//   * The PS5 blue stays the interactive accent; monochrome leads.
// ---------------------------------------------------------------------------

// ---- Raw brand constants (dark is the identity; accents shared) ----------
val PS5DarkBackground = Color(0xFF000000)   // AMOLED black
val PS5DarkSurface    = Color(0xFF0A0A0C)   // first elevation step
val PS5AccentBlue     = Color(0xFF2E8CFF)   // interactive accent

// Legacy aliases kept for non-composable references.
val PS5CardBackground = Color(0xFF0E0E10)

// Font families
val TitilliumFontFamily = FontFamily.Default
val PlayStationFontFamily = FontFamily.Default

// ---------------------------------------------------------------------------
// Semantic shell colors — the single theming contract.
// ---------------------------------------------------------------------------
@Immutable
data class PX5Colors(
    val background: Color,
    val surface: Color,
    val sheet: Color,
    val card: Color,
    val control: Color,
    val controlStrong: Color,
    val hairline: Color,
    val scrim: Color,
    val text: Color,
    val textSecondary: Color,
    val accent: Color,
    val accentGlow: Color,
    val teal: Color,
    val success: Color,
    val danger: Color,
    val infoMono: Color,
    val fadeTop: Color,
    val fadeBottom: Color,
    val backdropAlpha: Float
)

val DarkPX5Colors = PX5Colors(
    background      = PS5DarkBackground,
    surface         = PS5DarkSurface,
    sheet           = Color(0xFF101013),
    card            = Color(0xFF0E0E10),
    control         = Color(0xFF19191D),
    controlStrong   = Color(0xFF232328),
    hairline        = Color(0xFF1F1F24),
    scrim           = Color.Black.copy(alpha = 0.72f),
    text            = Color(0xFFECECEF),
    textSecondary   = Color(0xFF9B9BA3),
    accent          = PS5AccentBlue,
    accentGlow      = PS5AccentBlue,
    teal            = PS5AccentBlue,
    success         = Color(0xFF69F0AE),
    danger          = Color(0xFFFF5252),
    infoMono        = Color(0xFF8FD0FF),
    fadeTop         = Color.Black.copy(alpha = 0.5f),
    fadeBottom      = Color(0xFF000000),
    backdropAlpha   = 0.35f
)

val LightPX5Colors = PX5Colors(
    background      = Color(0xFFF4F5F7),
    surface         = Color(0xFFFFFFFF),
    sheet           = Color(0xFFFFFFFF),
    card            = Color(0xFFEDEEF1),
    control         = Color(0xFFE4E6EA),
    controlStrong   = Color(0xFFD6D9DF),
    hairline        = Color(0xFFE2E4E9),
    scrim           = Color.Black.copy(alpha = 0.5f),
    text            = Color(0xFF17181B),
    textSecondary   = Color(0xFF5F6368),
    accent          = PS5AccentBlue,
    accentGlow      = PS5AccentBlue,
    teal            = PS5AccentBlue,
    success         = Color(0xFF1B8A4B),
    danger          = Color(0xFFD32F2F),
    infoMono        = Color(0xFF0B6BC2),
    fadeTop         = Color.White.copy(alpha = 0.5f),
    fadeBottom      = Color(0xFFF4F5F7),
    backdropAlpha   = 0.10f
)

val LocalPX5Colors = staticCompositionLocalOf { DarkPX5Colors }

/** Convenience accessor for composables. */
@Composable
fun px5Colors(): PX5Colors = LocalPX5Colors.current

private fun px5Typography(c: PX5Colors): Typography = Typography(
    displayLarge = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Bold,
        fontSize = 36.sp,
        color = c.text
    ),
    displayMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Bold,
        fontSize = 28.sp,
        color = c.text
    ),
    titleLarge = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.SemiBold,
        fontSize = 22.sp,
        color = c.text
    ),
    titleMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.SemiBold,
        fontSize = 18.sp,
        color = c.text
    ),
    bodyLarge = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Normal,
        fontSize = 16.sp,
        color = c.text
    ),
    bodyMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        color = c.textSecondary
    ),
    labelMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.SemiBold,
        fontSize = 12.sp,
        color = c.textSecondary
    )
)

/**
 * PX5Theme — honors the persisted theme mode:
 *   0 = Dark (default)   1 = Light   2 = follow system setting.
 */
@Composable
fun PX5Theme(content: @Composable () -> Unit) {
    val mode = Px5Settings.themeMode.collectAsState().value
    val dark = when (mode) {
        1 -> false
        2 -> androidx.compose.foundation.isSystemInDarkTheme()
        else -> true
    }
    val colors = if (dark) DarkPX5Colors else LightPX5Colors

    val colorScheme = if (dark) {
        darkColorScheme(
            primary = colors.accent,
            secondary = colors.teal,
            background = colors.background,
            surface = colors.surface,
            onBackground = colors.text,
            onSurface = colors.text,
            surfaceVariant = PS5CardBackground,
            onSurfaceVariant = colors.textSecondary
        )
    } else {
        lightColorScheme(
            primary = colors.accent,
            secondary = colors.teal,
            background = colors.background,
            surface = colors.surface,
            onBackground = colors.text,
            onSurface = colors.text,
            surfaceVariant = Color(0xFFE9EBEF),
            onSurfaceVariant = colors.textSecondary
        )
    }

    CompositionLocalProvider(LocalPX5Colors provides colors) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = px5Typography(colors),
            content = content
        )
    }
}
