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
// PX5 palette — Steam Deck OS dark base with PS5 accent blend.
//
// Visual direction: Steam Deck OS-inspired dark UI with light PS5 accents.
//   * Deep navy/slate backgrounds  <- Steam Deck
//   * Elevated warm card surfaces  <- Steam Deck library cards
//   * PS5 blue stays primary; a teal secondary accents evidence/DIAG panels
// ---------------------------------------------------------------------------

// ---- Raw brand constants (dark is the identity; accents shared) ----------
val PS5DarkBackground = Color(0xFF121212)   // Vita3K dark
val PS5DarkSurface    = Color(0xFF1E1E1E)   // Vita3K surface
val PS5AccentBlue     = Color(0xFF0078D7)   // Vita3K blue

// Legacy aliases kept for non-composable references.
val PS5CardBackground = Color(0xFF1E1E1E)

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
    sheet           = PS5DarkSurface,
    card            = PS5DarkBackground,
    control         = Color(0xFF2A2A2A),
    controlStrong   = Color(0xFF333333),
    hairline        = Color(0xFF2A2A2A),
    scrim           = Color.Black.copy(alpha = 0.65f),
    text            = Color(0xFFE0E0E0),
    textSecondary   = Color(0xFFA0A0A0),
    accent          = PS5AccentBlue,
    accentGlow      = PS5AccentBlue,
    teal            = PS5AccentBlue,
    success         = Color(0xFF4CAF50),
    danger          = Color(0xFFF44336),
    infoMono        = Color(0xFF81D4FA),
    fadeTop         = Color.Black.copy(alpha = 0.5f),
    fadeBottom      = Color(0xFF121212),
    backdropAlpha   = 0.35f
)

val LightPX5Colors = PX5Colors(
    background      = Color(0xFFF5F5F5),
    surface         = Color(0xFFFFFFFF),
    sheet           = Color(0xFFFFFFFF),
    card            = Color(0xFFEEEEEE),
    control         = Color(0xFFE0E0E0),
    controlStrong   = Color(0xFFCCCCCC),
    hairline        = Color(0xFFE0E0E0),
    scrim           = Color.Black.copy(alpha = 0.5f),
    text            = Color(0xFF212121),
    textSecondary   = Color(0xFF757575),
    accent          = PS5AccentBlue,
    accentGlow      = PS5AccentBlue,
    teal            = PS5AccentBlue,
    success         = Color(0xFF4CAF50),
    danger          = Color(0xFFF44336),
    infoMono        = Color(0xFF0288D1),
    fadeTop         = Color.White.copy(alpha = 0.5f),
    fadeBottom      = Color(0xFFF5F5F5),
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
            surfaceVariant = Color(0xFFDCE3EE),
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
