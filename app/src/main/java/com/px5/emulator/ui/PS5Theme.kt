package com.px5.emulator.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.px5.emulator.R

// PS5 Aesthetic Colors
val PS5DarkBackground = Color(0xFF0B0E14)
val PS5DarkSurface = Color(0xFF161C26)
val PS5CardBackground = Color(0xFF1E2638)
val PS5AccentBlue = Color(0xFF0070D1)
val PS5AccentGlow = Color(0xFF2E8CFF)
val PS5TextPrimary = Color(0xFFFFFFFF)
val PS5TextSecondary = Color(0xFFA0AABF)
val PS5TrophyBronze = Color(0xFFCD7F32)
val PS5TrophySilver = Color(0xFFC0C0C0)
val PS5TrophyGold = Color(0xFFFFD700)

// ---------------------------------------------------------------------------
// Font families — sourced from PS5ish (https://github.com/SeaNaxxx/PS5ish)
//
// The previous font files (titillium_regular.ttf, titillium_bold.ttf,
// titillium_semibold.ttf, playstation.ttf) were CORRUPT — `file` reported
// invalid TrueType data. Loading them caused either:
//   - A Kotlin IllegalStateException (when FontFamily was SansSerif and
//     Compose tried to resolve an R.font.* ID that had no real font behind it)
//   - A native SIGSEGV (when FontFamily was bound to R.font.* and the
//     corrupt .ttf was actually parsed by Skia/HarfBuzz)
//
// Fix: replaced all 4 corrupt files with the correct ones from PS5ish:
//   titillium_regular.ttf   → titilliumweb_regular.ttf   (from PS5ish)
//   titillium_semibold.ttf  → titilliumweb_semibold.ttf  (from PS5ish)
//   titillium_bold.ttf      → titilliumweb_bold.ttf      (from PS5ish)
//   playstation.ttf         → playstation4.ttf            (from PS5ish)
//
// All 4 new files verified with `file` as valid TrueType Font data.
// ---------------------------------------------------------------------------
val TitilliumFontFamily = FontFamily(
    Font(R.font.titilliumweb_regular, weight = FontWeight.Normal),
    Font(R.font.titilliumweb_semibold, weight = FontWeight.SemiBold),
    Font(R.font.titilliumweb_bold, weight = FontWeight.Bold),
    // Explicit weight=700 mapping (the crash log showed Compose asking for
    // FontWeight(weight=700) which is Bold).
    Font(R.font.titilliumweb_bold, weight = FontWeight(700)),
    // Heavier weights fall back to Bold (the heaviest TitilliumWeb variant
    // we ship). This prevents crashes if any code requests Black/ExtraBold.
    Font(R.font.titilliumweb_bold, weight = FontWeight.Black),
    Font(R.font.titilliumweb_bold, weight = FontWeight.ExtraBold),
)

val PlayStationFontFamily = FontFamily(
    Font(R.font.playstation4, weight = FontWeight.Normal),
    Font(R.font.playstation4, weight = FontWeight.Medium),
    Font(R.font.playstation4, weight = FontWeight.SemiBold),
    Font(R.font.playstation4, weight = FontWeight.Bold),
    Font(R.font.playstation4, weight = FontWeight(700)),
    Font(R.font.playstation4, weight = FontWeight.Black),
    Font(R.font.playstation4, weight = FontWeight.ExtraBold),
)

val PS5Typography = Typography(
    displayLarge = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Bold,
        fontSize = 36.sp,
        color = PS5TextPrimary
    ),
    displayMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Bold,
        fontSize = 28.sp,
        color = PS5TextPrimary
    ),
    titleLarge = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.SemiBold,
        fontSize = 22.sp,
        color = PS5TextPrimary
    ),
    titleMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.SemiBold,
        fontSize = 18.sp,
        color = PS5TextPrimary
    ),
    bodyLarge = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Normal,
        fontSize = 16.sp,
        color = PS5TextPrimary
    ),
    bodyMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        color = PS5TextSecondary
    ),
    labelMedium = TextStyle(
        fontFamily = TitilliumFontFamily,
        fontWeight = FontWeight.SemiBold,
        fontSize = 12.sp,
        color = PS5TextSecondary
    )
)

@Composable
fun PX5Theme(content: @Composable () -> Unit) {
    val colorScheme = darkColorScheme(
        primary = PS5AccentBlue,
        secondary = PS5AccentGlow,
        background = PS5DarkBackground,
        surface = PS5DarkSurface,
        onBackground = PS5TextPrimary,
        onSurface = PS5TextPrimary,
        surfaceVariant = PS5CardBackground,
        onSurfaceVariant = PS5TextSecondary
    )

    MaterialTheme(
        colorScheme = colorScheme,
        typography = PS5Typography,
        content = content
    )
}
