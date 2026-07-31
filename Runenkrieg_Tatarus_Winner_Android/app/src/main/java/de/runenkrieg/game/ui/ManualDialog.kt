package de.runenkrieg.game.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.key
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import de.runenkrieg.game.model.Element

private const val MANUAL_PAGE_COUNT = 7

@Composable
fun ManualDialog(onDismiss: () -> Unit) {
    var page by rememberSaveable { mutableIntStateOf(0) }
    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(
            usePlatformDefaultWidth = false,
            dismissOnClickOutside = false
        )
    ) {
        Surface(
            modifier = Modifier
                .fillMaxSize()
                .safeDrawingPadding(),
            color = Color(0xFF090D18)
        ) {
            Column(modifier = Modifier.fillMaxSize()) {
                ManualHeader(page = page, onDismiss = onDismiss)
                ManualProgress(page = page)
                key(page) {
                    Column(
                        modifier = Modifier
                            .weight(1f)
                            .verticalScroll(rememberScrollState())
                            .padding(horizontal = 18.dp, vertical = 12.dp)
                            .widthIn(max = 720.dp)
                            .align(Alignment.CenterHorizontally)
                    ) {
                        when (page) {
                            0 -> GoalPage()
                            1 -> CardPage()
                            2 -> RoundPage()
                            3 -> StrengthPage()
                            4 -> ElementsPage()
                            5 -> MechanicsPage()
                            else -> StrategyPage()
                        }
                        Spacer(Modifier.height(24.dp))
                    }
                }
                ManualNavigation(
                    page = page,
                    onPrevious = { page = (page - 1).coerceAtLeast(0) },
                    onNext = {
                        if (page == MANUAL_PAGE_COUNT - 1) onDismiss()
                        else page += 1
                    }
                )
            }
        }
    }
}

@Composable
private fun ManualHeader(page: Int, onDismiss: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 18.dp, end = 8.dp, top = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Column {
            Text(
                "SPIELER-HANDBUCH",
                color = MaterialTheme.colorScheme.primary,
                fontWeight = FontWeight.Black,
                fontSize = 20.sp,
                letterSpacing = 1.5.sp
            )
            Text(
                "Seite ${page + 1} von $MANUAL_PAGE_COUNT",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.labelMedium
            )
        }
        TextButton(onClick = onDismiss) { Text("Schließen") }
    }
}

@Composable
private fun ManualProgress(page: Int) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 18.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        repeat(MANUAL_PAGE_COUNT) { index ->
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(4.dp)
                    .clip(CircleShape)
                    .background(
                        if (index <= page) MaterialTheme.colorScheme.primary
                        else MaterialTheme.colorScheme.surfaceVariant
                    )
            )
        }
    }
}

@Composable
private fun ManualNavigation(page: Int, onPrevious: () -> Unit, onNext: () -> Unit) {
    Surface(color = MaterialTheme.colorScheme.surface) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            OutlinedButton(
                onClick = onPrevious,
                enabled = page > 0,
                modifier = Modifier.weight(1f)
            ) {
                Text("Zurück")
            }
            Button(onClick = onNext, modifier = Modifier.weight(1f)) {
                Text(if (page == MANUAL_PAGE_COUNT - 1) "Spielen" else "Weiter")
            }
        }
    }
}

@Composable
private fun GoalPage() {
    PageTitle("So gewinnst du", "Bringe die Tokens von TATARUS auf 0, bevor deine eigenen aufgebraucht sind.")
    BattleGoalIllustration()
    ImportantBox(
        "Siegbedingung",
        "Du und TATARUS starten mit je 5 Tokens. Nach jeder Runde verändern Karten, Elemente und Mechaniken den Tokenstand. Sobald ein Spieler 0 Tokens hat, endet die Partie nach der Rundenauswertung."
    )
    SectionTitle("Das Grundprinzip")
    NumberedRule(1, "Du wählst zuerst eine Karte aus deiner Hand.")
    NumberedRule(2, "TATARUS kennt deine Karte und spielt eine Antwort – manchmal sogar eine Fusion.")
    NumberedRule(3, "Beide Karten erhalten ihre gesamte Stärke. Die höhere Stärke gewinnt den Stich.")
    NumberedRule(4, "Die Siegerkarte löst ihren Elementeffekt aus. Danach wirken die Mechaniken beider Karten.")
    NumberedRule(5, "Beide ziehen auf vier Handkarten nach und die nächste Runde beginnt.")
    TipBox("Nicht jede gewonnene Runde verursacht Schaden. Achte auf den Elementeffekt: Feuer und Eis schaden direkt, Wasser heilt dich und schadet gleichzeitig.")
}

@Composable
private fun BattleGoalIllustration() {
    IllustrationFrame {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly,
            verticalAlignment = Alignment.CenterVertically
        ) {
            TokenTower(label = "DU", count = 5, color = Color(0xFF0891B2))
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("⚔", fontSize = 35.sp)
                Text("SENKE AUF 0", color = Color(0xFFFCD34D), fontWeight = FontWeight.Black, fontSize = 11.sp)
            }
            TokenTower(label = "TATARUS", count = 0, color = Color(0xFFB91C1C))
        }
    }
}

@Composable
private fun TokenTower(label: String, count: Int, color: Color) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Box(
            modifier = Modifier
                .size(72.dp)
                .clip(CircleShape)
                .background(color),
            contentAlignment = Alignment.Center
        ) {
            Text(count.toString(), fontSize = 30.sp, fontWeight = FontWeight.Black)
        }
        Text("$label · TOKENS", fontWeight = FontWeight.Bold, fontSize = 11.sp)
    }
}

@Composable
private fun CardPage() {
    PageTitle("Eine Karte lesen", "Jede Information auf der Karte beeinflusst deine Entscheidung.")
    IllustrationFrame {
        ManualCard(
            symbol = "💧",
            ability = "Avatar",
            power = 13,
            element = "WASSER",
            type = "Beschwörung",
            mechanics = "Fusion · Resonanz · Wetter",
            colors = Color(0xFF1D4ED8) to Color(0xFF0891B2)
        )
    }
    Callout("1 · Fähigkeit", "Der Name oben links. Jede Fähigkeit besitzt einen festen Grundwert von 0 bis 13.")
    Callout("2 · Stärke", "Der Grundwert. Avatar ist 13, Funke ist 0. Boni werden erst in der Rundenauswertung addiert.")
    Callout("3 · Element", "Bestimmt Konterbonus, Heldenbonus, Wetterwirkung und Token-Effekt bei einem Sieg.")
    Callout("4 · Kartentyp", "Artefakt, Beschwörung, Runenstein, Verbündeter oder Segen/Fluch. Manche Typen geben zusätzliche Wertungsboni.")
    Callout("5 · Mechaniken", "Besondere Fähigkeiten am unteren Kartenrand. Sie können auch nach einer verlorenen Runde wirken.")
    TipBox("Hohe Grundstärke allein garantiert keinen Sieg. Ein passendes Element, Wetter und Held können mehrere Stärkepunkte Unterschied erzeugen.")
}

@Composable
private fun RoundPage() {
    PageTitle("Eine Runde spielen", "Vom Antippen der Handkarte bis zum Nachziehen.")
    FlowStep("①", "Wetterrisiko bedenken", "Das Wetter wird erst nach deiner Kartenwahl ausgelost. Regen stärkt Wasser und schwächt Feuer; Windsturm stärkt Luft und schwächt Erde. Wetterbindung ist deshalb eine bewusste Chance.")
    FlowArrow()
    FlowStep("②", "Karte antippen", "Tippe eine deiner vier Handkarten an. Karten sind gesperrt, während das Ergebnis angezeigt wird.")
    FlowArrow()
    FlowStep("③", "TATARUS-Antwort ansehen", "In der Arena erscheinen beide Karten. Der Text darunter zeigt Sieger und Gesamtstärke von dir und TATARUS.")
    FlowArrow()
    FlowStep("④", "Effekte lesen", "Violette Meldungen erklären Ketteneffekte, Überladung, Resonanz, Wetterbindung, Verbündete und Segen/Fluch.")
    FlowArrow()
    FlowStep("⑤", "Nächste Runde", "Tippe auf „Nächste Runde“. Beide Hände werden wieder auf vier Karten aufgefüllt.")
    ImportantBox(
        "Fusion bedienen",
        "Tippe eine Karte mit „Fusion“ an. Sie wird gelb markiert. Tippe eine zweite Fusionskarte an, um beide zu verschmelzen. Tippe die markierte Karte erneut an, wenn du sie ohne Fusion spielen möchtest."
    )
}

@Composable
private fun StrengthPage() {
    PageTitle("Wie Stärke berechnet wird", "Die Summe aller Boni entscheidet den Rundensieger.")
    IllustrationFrame {
        Column(modifier = Modifier.fillMaxWidth()) {
            FormulaRow("Grundwert", "0–13", Color(0xFF67E8F9))
            FormulaRow("+ Wetter & Risiko", "variabel", Color(0xFF93C5FD))
            FormulaRow("+ Elementkonter", "bis ±3", Color(0xFF86EFAC))
            FormulaRow("+ Heldenbonus", "+2 / +3", Color(0xFFC4B5FD))
            FormulaRow("+ Moral", "bis +4", Color(0xFFFCD34D))
            FormulaRow("+ Synergien", "variabel", Color(0xFFF9A8D4))
            HorizontalDivider(modifier = Modifier.padding(vertical = 8.dp))
            Text(
                "= GESAMTSTÄRKE",
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.Center,
                fontWeight = FontWeight.Black,
                fontSize = 18.sp
            )
        }
    }
    Callout("Grundwert", "Funke beginnt bei 0, Avatar endet bei 13. Fusion addiert die Grundwerte beider Karten, maximal bis Avatar.")
    Callout("Wetter & Risiko", "Überladung ist bei Rückstand wertvoller, kostet danach aber immer 1 Token. Segen/Fluch ist bei Rückstand stärker.")
    Callout("Elementkonter", "Ein günstiges Element gibt bis zu +3 Stärke; ein ungünstiges kann bis zu −3 kosten.")
    Callout("Heldenbonus", "Drache gibt +2 auf Feuer. Zauberer gibt +3 auf Magie.")
    Callout("Moral", "Für je 2 Tokens Vorsprung erhältst du +1 Stärke, maximal +4. Ein früher Vorsprung verstärkt sich daher.")
    Callout("Synergien", "Elementarresonanz, passende Elemente auf der Hand, Fusion, Ketten und bereits gespielte Karten können weitere Boni erzeugen.")
    WeatherStrip()
}

@Composable
private fun WeatherStrip() {
    SectionTitle("Wetterübersicht")
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(7.dp)
    ) {
        MiniInfoCard("🌧", "Regen", "Wasser +1\nFeuer −1", Modifier.weight(1f))
        MiniInfoCard("🌪", "Windsturm", "Luft +2\nErde −1", Modifier.weight(1f))
        MiniInfoCard("〰", "Erdbeben", "Keine direkte\nÄnderung", Modifier.weight(1f))
    }
}

@Composable
private fun ElementsPage() {
    PageTitle("Elemente und Token-Effekte", "Das Element entscheidet sowohl den Konter als auch die Belohnung für einen Sieg.")
    ImportantBox(
        "Konter lesen",
        "„Stark“ bedeutet einen positiven Stärkebonus gegen diese Elemente. „Schwach“ bedeutet einen Malus. Nicht aufgeführte Begegnungen sind neutral."
    )
    elementGuides.forEach { guide ->
        ElementGuideRow(guide)
    }
    TipBox("Für direkten Druck sind Wasser, Feuer, Eis und Schatten besonders wertvoll. Luft und Licht bauen dagegen schnell einen Token- und Moralvorsprung auf.")
}

private data class ElementGuide(
    val element: Element,
    val strong: String,
    val weak: String,
    val effect: String
)

private val elementGuides = listOf(
    ElementGuide(Element.FIRE, "Erde, Luft, Eis, Blitz, Schatten", "Wasser", "TATARUS verliert 1 Token"),
    ElementGuide(Element.WATER, "Feuer, Erde, Eis", "Luft, Blitz, Chaos", "+1 eigener und −1 TATARUS-Token"),
    ElementGuide(Element.EARTH, "Luft, Blitz, Eis, Chaos", "Wasser, Feuer", "+1 eigener Token"),
    ElementGuide(Element.AIR, "Wasser, Eis", "Erde, Feuer, Blitz, Schatten", "+2 eigene Tokens"),
    ElementGuide(Element.LIGHTNING, "Wasser, Erde, Feuer, Schatten", "Luft, Eis, Chaos", "+1 eigener Token"),
    ElementGuide(Element.ICE, "Feuer, Erde, Luft, Blitz", "Wasser, Chaos", "TATARUS verliert 1 Token"),
    ElementGuide(Element.MAGIC, "Feuer, Wasser, Erde, Luft, Blitz, Eis, Schatten", "Licht", "Kein direkter Tokeneffekt"),
    ElementGuide(Element.SHADOW, "Licht, Chaos", "Magie", "Stiehlt 1 TATARUS-Token"),
    ElementGuide(Element.LIGHT, "Schatten, Magie", "Chaos", "+2 eigene Tokens"),
    ElementGuide(Element.CHAOS, "Magie, Licht, Blitz", "Schatten, Feuer", "Gerade Runde: Vorteil; ungerade: Nachteil")
)

@Composable
private fun ElementGuideRow(guide: ElementGuide) {
    val colors = manualElementColors(guide.element)
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 5.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        border = BorderStroke(1.dp, colors.first.copy(alpha = 0.8f))
    ) {
        Row(
            modifier = Modifier.padding(10.dp),
            verticalAlignment = Alignment.Top
        ) {
            Box(
                modifier = Modifier
                    .size(48.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(Brush.linearGradient(listOf(colors.first, colors.second))),
                contentAlignment = Alignment.Center
            ) {
                Text(guide.element.symbol, fontSize = 25.sp)
            }
            Spacer(Modifier.width(10.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(guide.element.label, fontWeight = FontWeight.Black)
                Text("Stark: ${guide.strong}", fontSize = 12.sp, color = Color(0xFF86EFAC))
                Text("Schwach: ${guide.weak}", fontSize = 12.sp, color = Color(0xFFFCA5A5))
                Text("Bei Sieg: ${guide.effect}", fontSize = 12.sp, color = MaterialTheme.colorScheme.tertiary)
            }
        }
    }
}

@Composable
private fun MechanicsPage() {
    PageTitle("Mechaniken und Fusion", "Mechaniken verändern Stärke oder Tokens – einige wirken sogar bei einer Niederlage.")
    FusionIllustration()
    MechanicRule("⛓", "Ketteneffekte", "Gewinne zwei Runden hintereinander mit Kettenkarten. Der zweite Sieg entzieht zusätzlich 1 gegnerischen Token.")
    MechanicRule("◎", "Elementarresonanz", "Gleiche Elemente in Hand und Verlauf erhöhen die Stärke. Ab der dritten gespielten Karte desselben Elements gibt ein Sieg zusätzlich +1 Token.")
    MechanicRule("⚡", "Überladung", "Gibt bei Rückstand einen Wertungsbonus, kostet ihrem Besitzer nach dem Ausspielen aber immer 1 Token – unabhängig vom Sieger.")
    MechanicRule("◇", "Fusion", "Zwei Fusionskarten werden zu einer Karte. Ihre Grundwerte werden addiert, maximal 13; Mechaniken werden vereinigt.")
    MechanicRule("☁", "Wetterbindung", "Verdoppelt praktisch die Bedeutung des Wettermodifikators und kann nach der Runde zusätzlich Tokens geben oder nehmen.")
    MechanicRule("♟", "Verbündeter", "Bleibt nach dem Ausspielen eine Karte desselben Elements auf deiner Hand, erhältst du +1 Token.")
    MechanicRule("✦", "Segen/Fluch", "Hast du weniger Tokens, erhältst du +1. Sonst verliert TATARUS 1 Token.")
    TipBox("Überladung und Wetterbindung wirken auch ohne Rundensieg. Prüfe deshalb vor dem Ausspielen, ob der sichere Nebeneffekt den möglichen Stichverlust rechtfertigt.")
}

@Composable
private fun FusionIllustration() {
    IllustrationFrame {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            TinyCard("Nova", "6", "🔥", Color(0xFFB91C1C))
            Text("+", fontSize = 24.sp, fontWeight = FontWeight.Black)
            TinyCard("Supernova", "7", "🌱", Color(0xFF166534))
            Text("→", fontSize = 24.sp, fontWeight = FontWeight.Black)
            TinyCard("Avatar", "13", "🔥", Color(0xFF7C3AED))
        }
        Spacer(Modifier.height(8.dp))
        Text(
            "6 + 7 = 13 · Mechaniken werden zusammengeführt",
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Center,
            color = MaterialTheme.colorScheme.tertiary,
            fontWeight = FontWeight.Bold,
            fontSize = 12.sp
        )
    }
}

@Composable
private fun StrategyPage() {
    PageTitle("Der Weg zum Sieg", "Nutze diese Reihenfolge für jede Entscheidung.")
    StrategyStep("1", "Tokenstand", "Bei 1 Token ist Überladung lebensgefährlich. Bei Rückstand sind Segen/Fluch und Wasser besonders stark.")
    StrategyStep("2", "Wetterrisiko", "Das Wetter wird erst nach deiner Wahl sichtbar. Wasser und Luft profitieren von möglichen Wetterlagen; Feuer, Erde und Wetterbindung verlangen mehr Risikobereitschaft.")
    StrategyStep("3", "Schadensplan", "Willst du TATARUS sofort schwächen, suche Wasser, Feuer, Eis oder Schatten. Für einen dauerhaften Moralvorteil nutze Luft oder Licht.")
    StrategyStep("4", "Synergie", "Halte gleiche Elemente für Resonanz, passende Elementpaare und Fusionskarten zusammen. Spiele Kettenkarten möglichst direkt nacheinander.")
    StrategyStep("5", "Gesamtwert", "Vergleiche nicht nur die Zahl auf der Karte. Rechne Konter, Held, Wetter, Moral und mögliche Mechaniken grob hinzu.")
    StrategyStep("6", "TATARUS herausfordern", "TATARUS hält einen persistenten neuronalen Zustand und lernt aus Konsequenzen. Wechsle Elemente und Stärke, statt immer dieselbe Sequenz zu wiederholen.")
    StrategyStep("7", "Vorsprung sichern", "Mit Tokenvorsprung wächst dein Moralbonus. Vermeide dann unnötige Überladung und Chaos in ungeraden Runden.")
    SectionTitle("Schnelle Merkliste")
    Checklist("Wasser gegen Feuer: sehr stark und bester Tokenschwung.")
    Checklist("Drache + Feuer bzw. Zauberer + Magie: Heldenbonus nutzen.")
    Checklist("Fusion spart keine Karten, erzeugt aber extreme Stärke und vereinte Mechaniken.")
    Checklist("Chaos ist in geraden Runden hilfreich und in ungeraden Runden riskant.")
    Checklist("Der Text unter der Arena erklärt jeden Effekt – vor „Nächste Runde“ lesen.")
    ImportantBox(
        "TATARUS lernt mit",
        "Nach jeder Runde verändern Belohnung und lokale Eligibility-Spuren den synthetischen Nervenzustand. Im TATARUS-Labor kann das System weitere Duelle simulieren. Je länger ihr spielt, desto wichtiger werden Variation und vorausschauende Synergien."
    )
    Spacer(Modifier.height(8.dp))
    Text(
        "Du bist bereit für den Runenkrieg.",
        modifier = Modifier.fillMaxWidth(),
        textAlign = TextAlign.Center,
        color = MaterialTheme.colorScheme.primary,
        fontSize = 19.sp,
        fontWeight = FontWeight.Black
    )
}

@Composable
private fun PageTitle(title: String, subtitle: String) {
    Text(title, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Black)
    Spacer(Modifier.height(4.dp))
    Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant)
    Spacer(Modifier.height(16.dp))
}

@Composable
private fun SectionTitle(title: String) {
    Spacer(Modifier.height(16.dp))
    Text(
        title.uppercase(),
        color = MaterialTheme.colorScheme.primary,
        fontWeight = FontWeight.Black,
        letterSpacing = 1.sp,
        fontSize = 13.sp
    )
    Spacer(Modifier.height(7.dp))
}

@Composable
private fun IllustrationFrame(content: @Composable ColumnScope.() -> Unit) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 14.dp),
        shape = RoundedCornerShape(18.dp),
        color = Color(0xFF101B30),
        border = BorderStroke(1.dp, Color(0xFF334155))
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            content = content
        )
    }
}

@Composable
private fun ImportantBox(title: String, text: String) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        shape = RoundedCornerShape(12.dp),
        color = Color(0xFF164E63).copy(alpha = 0.46f),
        border = BorderStroke(1.dp, Color(0xFF22D3EE).copy(alpha = 0.5f))
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(title, color = Color(0xFF67E8F9), fontWeight = FontWeight.Black)
            Spacer(Modifier.height(3.dp))
            Text(text, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun TipBox(text: String) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 10.dp),
        shape = RoundedCornerShape(12.dp),
        color = Color(0xFF713F12).copy(alpha = 0.45f)
    ) {
        Row(modifier = Modifier.padding(12.dp), verticalAlignment = Alignment.Top) {
            Text("💡", fontSize = 21.sp)
            Spacer(Modifier.width(8.dp))
            Text(text, color = Color(0xFFFDE68A), style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun NumberedRule(number: Int, text: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        verticalAlignment = Alignment.Top
    ) {
        Box(
            modifier = Modifier
                .size(28.dp)
                .clip(CircleShape)
                .background(MaterialTheme.colorScheme.primary),
            contentAlignment = Alignment.Center
        ) {
            Text(number.toString(), color = Color(0xFF082F49), fontWeight = FontWeight.Black)
        }
        Spacer(Modifier.width(10.dp))
        Text(text, modifier = Modifier.weight(1f))
    }
}

@Composable
private fun Callout(title: String, text: String) {
    Column(modifier = Modifier.padding(vertical = 6.dp)) {
        Text(title, color = MaterialTheme.colorScheme.secondary, fontWeight = FontWeight.Bold)
        Text(text, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun ManualCard(
    symbol: String,
    ability: String,
    power: Int,
    element: String,
    type: String,
    mechanics: String,
    colors: Pair<Color, Color>
) {
    Box(
        modifier = Modifier
            .size(width = 176.dp, height = 234.dp)
            .clip(RoundedCornerShape(17.dp))
            .background(Brush.linearGradient(listOf(colors.first, colors.second)))
            .padding(12.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize(), verticalArrangement = Arrangement.SpaceBetween) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Column {
                    Text(ability, fontWeight = FontWeight.Black, fontSize = 20.sp)
                    Text("Stärke $power", fontSize = 12.sp)
                }
                Text(symbol, fontSize = 36.sp)
            }
            Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.fillMaxWidth()) {
                Text(element, fontWeight = FontWeight.Black, fontSize = 19.sp, letterSpacing = 1.sp)
                Text(type, fontSize = 12.sp)
            }
            Text(
                mechanics,
                modifier = Modifier.fillMaxWidth(),
                textAlign = TextAlign.Center,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold
            )
        }
    }
}

@Composable
private fun FlowStep(symbol: String, title: String, text: String) {
    Card(
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        modifier = Modifier.fillMaxWidth()
    ) {
        Row(modifier = Modifier.padding(13.dp), verticalAlignment = Alignment.Top) {
            Text(symbol, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Black, fontSize = 24.sp)
            Spacer(Modifier.width(10.dp))
            Column {
                Text(title, fontWeight = FontWeight.Black)
                Text(text, color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodyMedium)
            }
        }
    }
}

@Composable
private fun FlowArrow() {
    Text(
        "↓",
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        textAlign = TextAlign.Center,
        color = MaterialTheme.colorScheme.tertiary,
        fontWeight = FontWeight.Black
    )
}

@Composable
private fun FormulaRow(label: String, value: String, color: Color) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(label, color = color, fontWeight = FontWeight.Bold)
        Text(value, fontWeight = FontWeight.Black)
    }
}

@Composable
private fun MiniInfoCard(
    symbol: String,
    title: String,
    body: String,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier,
        color = MaterialTheme.colorScheme.surface,
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(8.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(symbol, fontSize = 24.sp)
            Text(title, fontWeight = FontWeight.Bold, fontSize = 12.sp)
            Text(body, fontSize = 10.sp, textAlign = TextAlign.Center, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun MechanicRule(symbol: String, title: String, text: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 7.dp),
        verticalAlignment = Alignment.Top
    ) {
        Box(
            modifier = Modifier
                .size(42.dp)
                .clip(RoundedCornerShape(11.dp))
                .background(Color(0xFF312E81)),
            contentAlignment = Alignment.Center
        ) {
            Text(symbol, color = Color(0xFFC4B5FD), fontSize = 21.sp)
        }
        Spacer(Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(title, color = Color(0xFFC4B5FD), fontWeight = FontWeight.Black)
            Text(text, color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun TinyCard(name: String, power: String, symbol: String, color: Color) {
    Box(
        modifier = Modifier
            .size(width = 64.dp, height = 91.dp)
            .clip(RoundedCornerShape(9.dp))
            .background(color)
            .padding(6.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.SpaceBetween,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(symbol, fontSize = 20.sp)
            Text(name, fontSize = 8.sp, fontWeight = FontWeight.Bold, textAlign = TextAlign.Center)
            Text(power, fontWeight = FontWeight.Black, fontSize = 17.sp)
        }
    }
}

@Composable
private fun StrategyStep(number: String, title: String, text: String) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 7.dp),
        verticalAlignment = Alignment.Top
    ) {
        Box(
            modifier = Modifier
                .size(35.dp)
                .clip(CircleShape)
                .background(Color(0xFF0E7490)),
            contentAlignment = Alignment.Center
        ) {
            Text(number, fontWeight = FontWeight.Black)
        }
        Spacer(Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(title, fontWeight = FontWeight.Black, color = MaterialTheme.colorScheme.primary)
            Text(text, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun Checklist(text: String) {
    Row(modifier = Modifier.padding(vertical = 5.dp), verticalAlignment = Alignment.Top) {
        Text("✓", color = Color(0xFF86EFAC), fontWeight = FontWeight.Black)
        Spacer(Modifier.width(8.dp))
        Text(text, modifier = Modifier.weight(1f))
    }
}

private fun manualElementColors(element: Element): Pair<Color, Color> = when (element) {
    Element.FIRE -> Color(0xFFB91C1C) to Color(0xFFEA580C)
    Element.WATER -> Color(0xFF1D4ED8) to Color(0xFF0891B2)
    Element.EARTH -> Color(0xFF166534) to Color(0xFF4D7C0F)
    Element.AIR -> Color(0xFF64748B) to Color(0xFF0E7490)
    Element.LIGHTNING -> Color(0xFFA16207) to Color(0xFFEAB308)
    Element.ICE -> Color(0xFF0E7490) to Color(0xFF3B82F6)
    Element.MAGIC -> Color(0xFF6D28D9) to Color(0xFF4338CA)
    Element.SHADOW -> Color(0xFF111827) to Color(0xFF581C87)
    Element.LIGHT -> Color(0xFFD97706) to Color(0xFFF59E0B)
    Element.CHAOS -> Color(0xFFBE123C) to Color(0xFFC026D3)
}
