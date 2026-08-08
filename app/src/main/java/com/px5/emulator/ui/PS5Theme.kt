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

val TitilliumFontFamily = FontFamily(
    Font(R.font.titillium_regular, FontWeight.Normal),
    Font(R.font.titillium_semibold, FontWeight.SemiBold),
    Font(R.font.titillium_bold, FontWeight.Bold)
)

val PlayStationFontFamily = FontFamily(
    Font(R.font.playstation, FontWeight.Normal)
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
