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
val PS5DarkBackground = Color(0xFF0E1218)   // Deck navy
val PS5DarkSurface    = Color(0xFF17202B)   // elevated slate surface
val PS5AccentBlue     = Color(0xFF0070D1)   // PS5 identity blue
val PS5TrophyBronze   = Color(0xFFCD7F32)
val PS5TrophySilver   = Color(0xFFC0C0C0)
val PS5TrophyGold     = Color(0xFFFFD700)

// Legacy aliases kept for non-composable references.
val PS5CardBackground = Color(0xFF1F2A38)

// Font families — uses the exact same definitions as the working PX5 repo.
// The res/font/titillium_web.xml font-family XML (synced from PX5 in this
// commit) maps FontWeight values to the correct .ttf files, so we don't
// need to enumerate every weight here — Compose resolves via the XML.
val TitilliumFontFamily = FontFamily(
    Font(R.font.titilliumweb_regular, FontWeight.Normal),
    Font(R.font.titilliumweb_semibold, FontWeight.SemiBold),
    Font(R.font.titilliumweb_bold, FontWeight.Bold)
)

val PlayStationFontFamily = FontFamily(
    Font(R.font.playstation4, FontWeight.Normal)
)

// ---------------------------------------------------------------------------
// Semantic shell colors — the single theming contract.
//
// Every UI surface reads tokens from LocalPX5Colors instead of raw
// constants, which is what makes the Light theme real rather than a
// reskin of text alone. Terminal-style boxes (log/report panels) are
// intentionally dark in BOTH themes, like Dolphin/Eden debug panels.
// ---------------------------------------------------------------------------
@Immutable
data class PX5Colors(
    val background: Color,
    val surface: Color,
    val sheet: Color,          // bottom sheets / dialogs
    val card: Color,           // translucent card fill
    val control: Color,        // icon buttons / secondary chips
    val controlStrong: Color,  // pressed-level fills, header buttons
    val hairline: Color,       // borders/dividers
    val scrim: Color,          // modal dim overlay
    val text: Color,
    val textSecondary: Color,
    val accent: Color,
    val accentGlow: Color,
    val teal: Color,
    val success: Color,
    val danger: Color,
    val infoMono: Color,       // monospace evidence text
    val fadeTop: Color,        // home ambient gradient
    val fadeBottom: Color,
    val backdropAlpha: Float   // home ambient art strength
)

val DarkPX5Colors = PX5Colors(
    background      = PS5DarkBackground,
    surface         = PS5DarkSurface,
    sheet           = Color(0xFF141A24),
    card            = Color.White.copy(alpha = 0.05f),
    control         = Color.White.copy(alpha = 0.08f),
    controlStrong   = Color.White.copy(alpha = 0.12f),
    hairline        = Color.White.copy(alpha = 0.13f),
    scrim           = Color.Black.copy(alpha = 0.65f),
    text            = Color(0xFFF2F5FA),
    textSecondary   = Color(0xFF9BA7BC),
    accent          = PS5AccentBlue,
    accentGlow      = Color(0xFF2E8CFF),
    teal            = Color(0xFF1FB6CD),
    success         = Color(0xFF69F0AE),
    danger          = Color(0xFFFF5252),
    infoMono        = Color(0xFF7DD3FC),
    fadeTop         = Color.Black.copy(alpha = 0.5f),
    fadeBottom      = Color(0xFF0B0E14),
    backdropAlpha   = 0.35f
)

val LightPX5Colors = PX5Colors(
    background      = Color(0xFFEFF2F7),
    surface         = Color(0xFFF8FAFD),
    sheet           = Color(0xFFFFFFFF),
    card            = Color(0xFF12203A).copy(alpha = 0.05f),
    control         = Color(0xFF12203A).copy(alpha = 0.07f),
    controlStrong   = Color(0xFF12203A).copy(alpha = 0.11f),
    hairline        = Color(0xFF12203A).copy(alpha = 0.14f),
    scrim           = Color.Black.copy(alpha = 0.45f),
    text            = Color(0xFF141A24),
    textSecondary   = Color(0xFF54617A),
    accent          = PS5AccentBlue,
    accentGlow      = Color(0xFF005CB8),
    teal            = Color(0xFF0B7E90),
    success         = Color(0xFF187B4B),
    danger          = Color(0xFFC62828),
    infoMono        = Color(0xFF0B5E8F),
    fadeTop         = Color.White.copy(alpha = 0.0f),
    fadeBottom      = Color(0xFFE4E9F1),
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
