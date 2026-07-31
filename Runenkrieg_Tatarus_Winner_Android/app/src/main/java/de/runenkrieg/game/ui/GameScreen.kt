package de.runenkrieg.game.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import de.runenkrieg.game.RunenkriegUiState
import de.runenkrieg.game.model.Element
import de.runenkrieg.game.model.EvaluationSummary
import de.runenkrieg.game.model.GamePhase
import de.runenkrieg.game.model.Hero
import de.runenkrieg.game.model.LearningSummary
import de.runenkrieg.game.model.OpponentMode
import de.runenkrieg.game.model.RuneCard
import de.runenkrieg.game.model.Winner
import java.util.Locale

@Composable
fun RunenkriegScreen(
    uiState: RunenkriegUiState,
    onPlayCard: (String) -> Unit,
    onContinue: () -> Unit,
    onNewGame: () -> Unit,
    onShowLearning: () -> Unit,
    onHideLearning: () -> Unit,
    onTrain: () -> Unit,
    onModeChange: (OpponentMode) -> Unit,
    onEvaluate: () -> Unit,
    onResetLearning: () -> Unit
) {
    val game = uiState.game
    var showManual by rememberSaveable { mutableStateOf(false) }
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(Color(0xFF090D18), Color(0xFF10192A), Color(0xFF090D18))
                )
            )
            .safeDrawingPadding()
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 14.dp, vertical = 10.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Header(
                learning = uiState.learning,
                isResolving = uiState.isResolving,
                isTraining = uiState.isTraining,
                isEvaluating = uiState.isEvaluating,
                onShowLearning = onShowLearning,
                onShowManual = { showManual = true },
                onNewGame = onNewGame
            )

            Spacer(Modifier.height(12.dp))
            PlayerBar(
                title = "TATARUS",
                hero = game.aiHero,
                tokens = game.aiTokens,
                alignEnd = true
            )
            Spacer(Modifier.height(10.dp))
            OpponentHand(game.aiHand.size)
            Spacer(Modifier.height(12.dp))

            WeatherBanner(
                weather = game.weather?.let { "${it.symbol} ${it.label}" } ?: "◇ Wetter wird ausgelost",
                round = game.round.coerceAtMost(100)
            )
            Spacer(Modifier.height(10.dp))

            BattleArena(
                aiCard = game.aiCard,
                playerCard = game.playerCard,
                winner = game.roundWinner
            )
            Spacer(Modifier.height(10.dp))

            StatusPanel(
                status = game.status,
                mechanicMessages = game.mechanicMessages
            )

            if (game.phase == GamePhase.REVEAL) {
                Spacer(Modifier.height(10.dp))
                Button(
                    onClick = onContinue,
                    enabled =
                        !uiState.isResolving &&
                            !uiState.isTraining &&
                            !uiState.isEvaluating,
                    modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary)
                ) {
                    Text("Nächste Runde", fontWeight = FontWeight.Bold)
                }
            }

            Spacer(Modifier.height(12.dp))
            Text(
                text = if (game.fusionSelectionId == null) "DEINE HAND" else "ZWEITE FUSIONSKARTE WÄHLEN",
                style = MaterialTheme.typography.labelLarge,
                color = if (game.fusionSelectionId == null) {
                    MaterialTheme.colorScheme.onSurfaceVariant
                } else MaterialTheme.colorScheme.tertiary
            )
            Spacer(Modifier.height(7.dp))
            LazyRow(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                items(game.playerHand, key = { it.id }) { card ->
                    val selected = card.id == game.fusionSelectionId
                    val legalFusionTarget = game.fusionSelectionId == null ||
                        selected ||
                        de.runenkrieg.game.model.Mechanic.FUSION in card.mechanics
                    RuneCardView(
                        card = card,
                        selected = selected,
                        enabled =
                            game.phase == GamePhase.PLAYER_TURN &&
                                legalFusionTarget &&
                                !uiState.isResolving &&
                                !uiState.isTraining &&
                                !uiState.isEvaluating,
                        onClick = { onPlayCard(card.id) }
                    )
                }
            }
            Spacer(Modifier.height(12.dp))
            PlayerBar(
                title = "Du",
                hero = game.playerHero,
                tokens = game.playerTokens,
                alignEnd = false
            )
            Spacer(Modifier.height(18.dp))
        }

        if (game.phase == GamePhase.GAME_OVER) {
            GameOverDialog(
                status = game.status,
                playerTokens = game.playerTokens,
                aiTokens = game.aiTokens,
                rounds = game.history.size,
                onNewGame = onNewGame
            )
        }

        if (uiState.showLearningPanel) {
            LearningDialog(
                summary = uiState.learning,
                isResolving = uiState.isResolving,
                isTraining = uiState.isTraining,
                progress = uiState.trainingProgress,
                target = uiState.trainingTarget,
                isEvaluating = uiState.isEvaluating,
                evaluationProgress = uiState.evaluationProgress,
                evaluationTarget = uiState.evaluationTarget,
                evaluation = uiState.evaluation,
                onDismiss = onHideLearning,
                onTrain = onTrain,
                onModeChange = onModeChange,
                onEvaluate = onEvaluate,
                onReset = onResetLearning
            )
        }

        if (showManual) {
            ManualDialog(onDismiss = { showManual = false })
        }
    }
}

@Composable
private fun Header(
    learning: LearningSummary,
    isResolving: Boolean,
    isTraining: Boolean,
    isEvaluating: Boolean,
    onShowLearning: () -> Unit,
    onShowManual: () -> Unit,
    onNewGame: () -> Unit
) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    "RUNENKRIEG",
                    fontSize = 25.sp,
                    fontWeight = FontWeight.Black,
                    color = MaterialTheme.colorScheme.primary,
                    letterSpacing = 2.sp
                )
                Text(
                    if (isResolving) {
                        "TATARUS verarbeitet die neuronale Entscheidung …"
                    } else if (isEvaluating) {
                        "TATARUS wird auf Testseeds evaluiert …"
                    } else if (isTraining) {
                        "TATARUS trainiert im Hintergrund …"
                    } else {
                        "${learning.observations} TATARUS-Erfahrungen"
                    },
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            TextButton(onClick = onNewGame) { Text("Neu") }
        }
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.End
        ) {
            TextButton(onClick = onShowManual) { Text("Handbuch") }
            TextButton(onClick = onShowLearning) { Text("TATARUS-Labor") }
        }
    }
}

@Composable
private fun PlayerBar(title: String, hero: Hero, tokens: Int, alignEnd: Boolean) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.surfaceVariant)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            if (!alignEnd) TokenOrb(tokens)
            Column(
                modifier = Modifier.weight(1f).padding(horizontal = 12.dp),
                horizontalAlignment = if (alignEnd) Alignment.Start else Alignment.End
            ) {
                Text(title, fontWeight = FontWeight.Bold, fontSize = 17.sp)
                Text(
                    "${hero.label} · +${hero.bonus} auf ${hero.element.label}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            if (alignEnd) TokenOrb(tokens)
        }
    }
}

@Composable
private fun TokenOrb(tokens: Int) {
    Box(
        modifier = Modifier
            .size(54.dp)
            .clip(CircleShape)
            .background(if (tokens > 2) Color(0xFF164E63) else Color(0xFF7F1D1D)),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(tokens.toString(), fontWeight = FontWeight.Black, fontSize = 20.sp)
            Text("TOKEN", fontSize = 8.sp, letterSpacing = 1.sp)
        }
    }
}

@Composable
private fun OpponentHand(count: Int) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.Center
    ) {
        repeat(count) { index ->
            if (index > 0) Spacer(Modifier.width(7.dp))
            CardBack()
        }
    }
}

@Composable
private fun CardBack() {
    Box(
        modifier = Modifier
            .size(width = 58.dp, height = 82.dp)
            .clip(RoundedCornerShape(9.dp))
            .background(
                Brush.linearGradient(
                    listOf(Color(0xFF312E81), Color(0xFF111827), Color(0xFF164E63))
                )
            ),
        contentAlignment = Alignment.Center
    ) {
        Box(
            modifier = Modifier
                .size(38.dp)
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.32f)),
            contentAlignment = Alignment.Center
        ) {
            Text("ᚱ", color = Color(0xFF67E8F9), fontSize = 24.sp)
        }
    }
}

@Composable
private fun WeatherBanner(weather: String, round: Int) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text("Runde $round", fontWeight = FontWeight.Bold)
        Surface(
            shape = RoundedCornerShape(50),
            color = MaterialTheme.colorScheme.surfaceVariant
        ) {
            Text(weather, modifier = Modifier.padding(horizontal = 13.dp, vertical = 6.dp))
        }
    }
}

@Composable
private fun BattleArena(
    aiCard: RuneCard?,
    playerCard: RuneCard?,
    winner: Winner?
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = Color(0xFF0F172A).copy(alpha = 0.86f),
        border = BorderStroke(1.dp, Color(0xFF334155))
    ) {
        Row(
            modifier = Modifier.padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            ArenaSlot(
                label = "TATARUS",
                card = aiCard,
                won = winner == Winner.AI
            )
            Text(
                "VS",
                fontWeight = FontWeight.Black,
                fontSize = 21.sp,
                color = MaterialTheme.colorScheme.tertiary,
                modifier = Modifier.padding(horizontal = 6.dp)
            )
            ArenaSlot(
                label = "DU",
                card = playerCard,
                won = winner == Winner.PLAYER
            )
        }
    }
}

@Composable
private fun ArenaSlot(label: String, card: RuneCard?, won: Boolean) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            label,
            style = MaterialTheme.typography.labelMedium,
            color = if (won) Color(0xFF86EFAC) else MaterialTheme.colorScheme.onSurfaceVariant
        )
        Spacer(Modifier.height(5.dp))
        if (card == null) {
            Box(
                modifier = Modifier
                    .size(width = 112.dp, height = 148.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(Color(0xFF1E293B).copy(alpha = 0.6f)),
                contentAlignment = Alignment.Center
            ) {
                Text("ᚱ", color = Color(0xFF475569), fontSize = 38.sp)
            }
        } else {
            RuneCardView(
                card = card,
                compact = true,
                selected = won,
                enabled = false
            )
        }
    }
}

@Composable
private fun StatusPanel(status: String, mechanicMessages: List<String>) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp),
        color = MaterialTheme.colorScheme.surface
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(status, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
            mechanicMessages.forEach {
                Spacer(Modifier.height(4.dp))
                Text(
                    "• $it",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.secondary
                )
            }
        }
    }
}

@Composable
private fun RuneCardView(
    card: RuneCard,
    compact: Boolean = false,
    selected: Boolean,
    enabled: Boolean,
    onClick: () -> Unit = {}
) {
    val (firstColor, secondColor) = elementColors(card.element)
    val width = if (compact) 112.dp else 148.dp
    val height = if (compact) 148.dp else 206.dp
    Box(
        modifier = Modifier
            .size(width, height)
            .clip(RoundedCornerShape(if (compact) 12.dp else 15.dp))
            .background(Brush.linearGradient(listOf(firstColor, secondColor)))
            .then(
                if (selected) Modifier.background(Color.White.copy(alpha = 0.16f))
                else Modifier
            )
            .clickable(enabled = enabled, onClick = onClick)
            .padding(if (compact) 8.dp else 10.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.SpaceBetween
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.Top
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        card.ability.label,
                        fontWeight = FontWeight.Black,
                        fontSize = if (compact) 13.sp else 17.sp,
                        color = Color.White,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        "Stärke ${card.ability.power}",
                        fontSize = if (compact) 9.sp else 11.sp,
                        color = Color.White.copy(alpha = 0.82f)
                    )
                }
                Text(card.element.symbol, fontSize = if (compact) 22.sp else 29.sp)
            }

            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    card.element.label.uppercase(Locale.GERMAN),
                    fontWeight = FontWeight.Black,
                    letterSpacing = 1.sp,
                    fontSize = if (compact) 13.sp else 16.sp,
                    color = Color.White
                )
                Text(
                    card.type.label,
                    fontSize = if (compact) 9.sp else 11.sp,
                    color = Color.White.copy(alpha = 0.8f),
                    maxLines = 1
                )
                if (card.fused) {
                    Text("FUSION", fontSize = 9.sp, fontWeight = FontWeight.Black, color = Color(0xFFFDE68A))
                }
            }

            Text(
                card.mechanics.take(if (compact) 1 else 2).joinToString(" · ") { it.label },
                modifier = Modifier.fillMaxWidth(),
                fontSize = if (compact) 8.sp else 9.sp,
                lineHeight = if (compact) 9.sp else 11.sp,
                textAlign = TextAlign.Center,
                color = Color.White.copy(alpha = 0.9f),
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
        }
        if (!enabled && !compact) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.35f))
            )
        }
        if (selected) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(2.dp)
            )
        }
    }
}

private fun elementColors(element: Element): Pair<Color, Color> = when (element) {
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

@Composable
private fun GameOverDialog(
    status: String,
    playerTokens: Int,
    aiTokens: Int,
    rounds: Int,
    onNewGame: () -> Unit
) {
    AlertDialog(
        onDismissRequest = {},
        title = { Text("Das Duell ist entschieden", fontWeight = FontWeight.Black) },
        text = {
            Column {
                Text(status)
                Spacer(Modifier.height(12.dp))
                Text("Endstand: Du $playerTokens · TATARUS $aiTokens")
                Text("Gespielte Runden: $rounds")
                Spacer(Modifier.height(8.dp))
                Text(
                    "Jede Runde dieser Partie ist bereits in den lokalen TATARUS-Zustand eingeflossen.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        },
        confirmButton = {
            Button(onClick = onNewGame) { Text("Neues Spiel") }
        }
    )
}

@Composable
private fun LearningDialog(
    summary: LearningSummary,
    isResolving: Boolean,
    isTraining: Boolean,
    progress: Int,
    target: Int,
    isEvaluating: Boolean,
    evaluationProgress: Int,
    evaluationTarget: Int,
    evaluation: EvaluationSummary?,
    onDismiss: () -> Unit,
    onTrain: () -> Unit,
    onModeChange: (OpponentMode) -> Unit,
    onEvaluate: () -> Unit,
    onReset: () -> Unit
) {
    var confirmReset by remember { mutableStateOf(false) }
    val busy = isResolving || isTraining || isEvaluating
    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.94f)
                .padding(horizontal = 14.dp, vertical = 10.dp)
                .widthIn(max = 560.dp)
                .safeDrawingPadding(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
        ) {
            Column(
                modifier = Modifier.fillMaxSize()
            ) {
                Column(modifier = Modifier.padding(start = 18.dp, end = 18.dp, top = 18.dp)) {
                    Text(
                        "TATARUS – eingefrorener Gewinner",
                        style = MaterialTheme.typography.headlineSmall,
                        fontWeight = FontWeight.Black
                    )
                    Text(
                        "Vorregistriert ausgewählt und unabhängig repliziert",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        "Seed 20260732 · 10.000 Runden · Snapshot SHA-256 98c567…f71668",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.primary,
                        fontWeight = FontWeight.Bold
                    )
                }

                Column(
                    modifier = Modifier
                        .weight(1f)
                        .verticalScroll(rememberScrollState())
                        .padding(horizontal = 18.dp)
                ) {
                    Spacer(Modifier.height(14.dp))
                    SectionTitle("Betriebsart")
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .horizontalScroll(rememberScrollState())
                    ) {
                        listOf(OpponentMode.FROZEN_TATARUS).forEach { mode ->
                            if (mode == summary.mode) {
                                Button(
                                    onClick = {},
                                    enabled = !busy,
                                    modifier = Modifier.padding(end = 8.dp)
                                ) {
                                    Text(mode.label)
                                }
                            } else {
                                OutlinedButton(
                                    onClick = { onModeChange(mode) },
                                    enabled = !busy,
                                    modifier = Modifier.padding(end = 8.dp)
                                ) {
                                    Text(mode.label)
                                }
                            }
                        }
                    }
                    Text(
                        summary.mode.description,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )

                    SectionTitle("Spielerfolg")
                    Metric("Lernbeobachtungen", summary.observations.toString())
                    Metric(
                        "Reale Runden",
                        summary.realObservations.toString(),
                        "Nur tatsächlich gespielte Runden, getrennt vom Selbsttraining."
                    )
                    val realOutcomes =
                        summary.realWins + summary.realDraws + summary.realLosses
                    Metric(
                        "Reale Rundensiegrate",
                        if (realOutcomes == 0L) "–" else {
                            "${(summary.realWins * 100.0 / realOutcomes).toInt()} %"
                        },
                        "Siege / alle realen Runden; kein Selbsttraining."
                    )
                    Metric(
                        "Reale Belohnung",
                        if (summary.realObservations == 0L) "–" else {
                            "${(summary.realAverageReward * 100).toInt()} %"
                        }
                    )
                    Metric("Selbsttrainingsrunden", summary.trainingObservations.toString())
                    Metric(
                        "Trainingsbelohnung",
                        if (summary.trainingObservations == 0L) "–" else {
                            "${(summary.trainingAverageReward * 100).toInt()} %"
                        }
                    )
                    Metric("Kontext-Hashes (64 Bit)", summary.contexts.toString())
                    Metric("Bewertete Aktionsklassen", summary.actions.toString())
                    Metric("Erkundungsrate", "${(summary.explorationRate * 100).toInt()} %")
                    Metric("Trainingsbatches", summary.trainingRuns.toString())

                    SectionTitle("Nervensystem")
                    Metric("Neuronen", summary.totalNeurons.toString())
                    Metric("Eingabekanäle", summary.inputChannels.toString())
                    Metric(
                        "Neuronale Eingangsprojektionen",
                        summary.inputProjections.toString(),
                        "Jeder der 128 Kanäle projiziert deterministisch auf acht Neuronen."
                    )
                    Metric("Neuronale Schritte", summary.neuralSteps.toString())
                    Metric("Spikes", summary.neuralSpikes.toString())
                    Metric(
                        "Mittlere Feuerrate",
                        formatNumber(summary.spikeRateHz, 2) + " Hz",
                        "Populationsmittel; Zielwert des LargeScale-Kerns: 6 Hz."
                    )
                    Metric("Übertragungen", summary.transmissions.toString())
                    Metric("Synapsen gesamt", summary.totalSynapses.toString())
                    Metric(
                        "Dynamischer Zustand",
                        formatBytes(summary.stateBytesEstimate),
                        "Schätzung der numerischen Kernzustände; Dateigröße ist gzip-komprimiert."
                    )
                    Metric(
                        "Mittlere Entscheidungszeit",
                        if (summary.measuredDecisions == 0L) "–"
                        else formatNumber(summary.averageDecisionMillis, 2) + " ms",
                        "Auf diesem Gerät gemessen, inklusive neuronaler Gegenfaktual-Rollouts."
                    )
                    Metric(
                        "Kürzlich aktive Synapsen",
                        summary.recentlyActiveSynapses.toString(),
                        "Synapsen mit hinreichender abklingender Übertragungsnutzung."
                    )
                    Metric(
                        "Gewichte an Grenze",
                        formatPercent(summary.saturatedWeightFraction)
                    )

                    SectionTitle("Repräsentationen")
                    Metric("Assemblies", summary.activeAssemblies.toString())
                    Metric(
                        "Assembly-Entropie",
                        formatNumber(summary.assemblyEntropy, 3),
                        "0 = kollabierte Belegung, 1 = gleichmäßige Nutzung."
                    )
                    Metric(
                        "Assembly-Trennung",
                        formatNumber(summary.assemblySeparation, 3),
                        "Mittlere Distanz zwischen normalisierten Prototypen."
                    )
                    Metric("Reaktivierungen", summary.assemblyReactivations.toString())

                    SectionTitle("Energie")
                    Metric("Mittlere Energie", formatPercent(summary.meanEnergy))
                    Metric("10-%-Perzentil", formatPercent(summary.energyP10))
                    Metric("Minimum", formatPercent(summary.minimumEnergy))
                    Metric(
                        "Kosten je Beobachtung",
                        formatNumber(summary.energyCostPerObservation, 4)
                    )

                    SectionTitle("Eligibility")
                    Metric(
                        "Vorzeichenmittel",
                        formatNumber(summary.meanEligibility, 4),
                        "Kann sich durch positive und negative Spuren aufheben."
                    )
                    Metric("Mittlerer Betrag", formatNumber(summary.meanAbsoluteEligibility, 4))
                    Metric("Standardabweichung", formatNumber(summary.eligibilityStdDev, 4))
                    Metric("Maximaler Betrag", formatNumber(summary.maximumAbsoluteEligibility, 4))
                    Metric("Aktive Spuren", formatPercent(summary.activeEligibilityFraction))
                    Metric(
                        "Positive / negative",
                        "${formatPercent(summary.positiveEligibilityFraction)} / " +
                            formatPercent(summary.negativeEligibilityFraction)
                    )
                    Metric(
                        "Eligibility-Sättigung",
                        formatPercent(summary.saturatedEligibilityFraction)
                    )

                    if (isTraining && target > 0) {
                        SectionTitle("Selbsttraining")
                        Text("$progress / $target", style = MaterialTheme.typography.labelLarge)
                        Spacer(Modifier.height(6.dp))
                        LinearProgressIndicator(
                            progress = { (progress.toFloat() / target).coerceIn(0f, 1f) },
                            modifier = Modifier.fillMaxWidth()
                        )
                    }

                    if (isEvaluating && evaluationTarget > 0) {
                        SectionTitle("Lernfreie Evaluation")
                        Text(
                            "$evaluationProgress / $evaluationTarget Spiele",
                            style = MaterialTheme.typography.labelLarge
                        )
                        Spacer(Modifier.height(6.dp))
                        LinearProgressIndicator(
                            progress = {
                                (evaluationProgress.toFloat() / evaluationTarget)
                                    .coerceIn(0f, 1f)
                            },
                            modifier = Modifier.fillMaxWidth()
                        )
                    }

                    evaluation?.let {
                        SectionTitle("Letzte Mehrseed-Evaluation")
                        Text(
                            "${it.gamesPerMode} identische Testseeds je Modus · Lernen deaktiviert",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        SuperiorityStatus(it)
                        Spacer(Modifier.height(8.dp))
                        it.results.sortedByDescending { result -> result.winRate }
                            .forEach { result ->
                                EvaluationResultCard(result)
                                Spacer(Modifier.height(8.dp))
                            }
                    }
                    Spacer(Modifier.height(12.dp))
                }

                HorizontalDivider(color = MaterialTheme.colorScheme.surfaceVariant)
                Column(modifier = Modifier.padding(14.dp)) {
                    Button(
                        onClick = onTrain,
                        enabled = !busy && summary.mode.trainable,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            "Gewinner-Snapshot ist eingefroren"
                        )
                    }
                    OutlinedButton(
                        onClick = onEvaluate,
                        enabled = !busy,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            if (isEvaluating) "Evaluation läuft …"
                            else "Eingefrorenes Modell evaluieren"
                        )
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        TextButton(
                            onClick = { confirmReset = true },
                            enabled = false
                        ) {
                            Text("Snapshot geschützt")
                        }
                        TextButton(onClick = onDismiss) {
                            Text("Schließen")
                        }
                    }
                }
            }
        }
    }

    if (confirmReset) {
        AlertDialog(
            onDismissRequest = { confirmReset = false },
            title = { Text("TATARUS-Zustand löschen?") },
            text = {
                Text(
                    "Alle lokalen neuronalen Zustände, Synapsenspuren, Assemblies und Erfahrungen werden dauerhaft entfernt."
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        confirmReset = false
                        onReset()
                    }
                ) { Text("Löschen", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { confirmReset = false }) { Text("Abbrechen") }
            }
        )
    }
}

@Composable
private fun SectionTitle(title: String) {
    Spacer(Modifier.height(18.dp))
    Text(
        title.uppercase(Locale.GERMAN),
        style = MaterialTheme.typography.labelLarge,
        color = MaterialTheme.colorScheme.secondary,
        fontWeight = FontWeight.Black
    )
    Spacer(Modifier.height(5.dp))
}

@Composable
private fun Metric(label: String, value: String, hint: String? = null) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 7.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f).padding(end = 10.dp)) {
            Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
            hint?.let {
                Text(
                    it,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.72f)
                )
            }
        }
        Text(value, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
    }
    HorizontalDivider(color = MaterialTheme.colorScheme.surfaceVariant)
}

@Composable
private fun SuperiorityStatus(evaluation: EvaluationSummary) {
    val tatarus =
        evaluation.results.firstOrNull { it.mode == OpponentMode.PURE_TATARUS }
    val rules =
        evaluation.results.firstOrNull { it.mode == OpponentMode.RULE_ONLY }
    if (tatarus == null || rules == null) return
    val favorable =
        tatarus.winRate > rules.winRate &&
            tatarus.averageTokenSwing > rules.averageTokenSwing
    val message = when {
        evaluation.gamesPerMode < 20 && favorable ->
            "Exploratives Überlegenheitssignal: Siegrate und Token-Swing liegen " +
                "über der Regelbaseline. Für eine Behauptung sind Holdout und " +
                "mindestens 20 Seeds erforderlich."
        evaluation.gamesPerMode < 20 ->
            "Überlegenheit nicht bestätigt. Dieser Schnelllauf besitzt zu wenige " +
                "Seeds für eine wissenschaftliche Behauptung."
        favorable ->
            "Überlegenheitskandidat in beiden primären Metriken; Signifikanz- und " +
                "Replikationsprüfung bleiben erforderlich."
        else ->
            "Überlegenheit gegenüber der Regelbaseline nicht bestätigt."
    }
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 8.dp),
        shape = RoundedCornerShape(10.dp),
        color = if (favorable) {
            MaterialTheme.colorScheme.primaryContainer
        } else {
            MaterialTheme.colorScheme.surfaceVariant
        }
    ) {
        Text(
            message,
            modifier = Modifier.padding(10.dp),
            style = MaterialTheme.typography.bodySmall,
            color = if (favorable) {
                MaterialTheme.colorScheme.onPrimaryContainer
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            }
        )
    }
}

@Composable
private fun EvaluationResultCard(result: de.runenkrieg.game.model.EvaluationModeResult) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.45f)
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(result.mode.label, fontWeight = FontWeight.Bold)
                Text(
                    formatPercent(result.winRate),
                    color = MaterialTheme.colorScheme.primary,
                    fontWeight = FontWeight.Black
                )
            }
            Text(
                "S/U/N ${result.wins}/${result.draws}/${result.losses} · " +
                    "Swing ${formatNumber(result.averageTokenSwing, 2)} · " +
                    "Ø ${formatNumber(result.averageRounds, 1)} Runden",
                style = MaterialTheme.typography.bodySmall
            )
            Text(
                "Spikes ${formatNumber(result.spikesPerGame, 1)} · " +
                    "Übertragungen ${formatNumber(result.transmissionsPerGame, 1)} · " +
                    "Energie ${formatNumber(result.energyCostPerGame, 3)} · " +
                    "Entscheidung ${formatNumber(result.averageDecisionMillis, 2)} ms",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

private fun formatPercent(value: Double): String =
    String.format(Locale.GERMAN, "%.1f %%", value * 100.0)

private fun formatNumber(value: Double, decimals: Int): String =
    String.format(Locale.GERMAN, "%.${decimals}f", value)

private fun formatBytes(bytes: Long): String {
    val mebibytes = bytes / (1024.0 * 1024.0)
    return String.format(Locale.GERMAN, "%.2f MiB", mebibytes)
}
