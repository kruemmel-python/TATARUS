package de.runenkrieg.game

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.lifecycle.viewmodel.compose.viewModel
import de.runenkrieg.game.ui.RunenkriegScreen
import de.runenkrieg.game.ui.RunenkriegTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            RunenkriegTheme {
                val gameViewModel: GameViewModel = viewModel()
                val state by gameViewModel.uiState.collectAsState()
                RunenkriegScreen(
                    uiState = state,
                    onPlayCard = gameViewModel::playCard,
                    onContinue = gameViewModel::continueRound,
                    onNewGame = gameViewModel::newGame,
                    onShowLearning = { gameViewModel.setLearningPanel(true) },
                    onHideLearning = { gameViewModel.setLearningPanel(false) },
                    onTrain = { gameViewModel.trainAi(250) },
                    onModeChange = gameViewModel::setOpponentMode,
                    onEvaluate = { gameViewModel.evaluateAi(5) },
                    onResetLearning = gameViewModel::resetLearning
                )
            }
        }
    }
}
