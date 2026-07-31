package de.runenkrieg.game

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import de.runenkrieg.game.ai.GameOpponent
import de.runenkrieg.game.ai.FrozenTensorFlowAi
import de.runenkrieg.game.engine.GameEngine
import de.runenkrieg.game.model.EvaluationSummary
import de.runenkrieg.game.model.GameState
import de.runenkrieg.game.model.LearningSummary
import de.runenkrieg.game.model.OpponentMode
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class RunenkriegUiState(
    val game: GameState,
    val learning: LearningSummary,
    val showLearningPanel: Boolean = false,
    val isResolving: Boolean = false,
    val isTraining: Boolean = false,
    val trainingProgress: Int = 0,
    val trainingTarget: Int = 0,
    val isEvaluating: Boolean = false,
    val evaluationProgress: Int = 0,
    val evaluationTarget: Int = 0,
    val evaluation: EvaluationSummary? = null
)

class GameViewModel(application: Application) : AndroidViewModel(application) {
    private val ai: GameOpponent = FrozenTensorFlowAi(application)
    private val engine = GameEngine(ai)

    private val _uiState = MutableStateFlow(
        RunenkriegUiState(
            game = engine.newGame(),
            learning = ai.summary()
        )
    )
    val uiState: StateFlow<RunenkriegUiState> = _uiState.asStateFlow()

    fun playCard(cardId: String) {
        val before = _uiState.value
        if (before.isResolving || before.isTraining || before.isEvaluating) return
        _uiState.update { it.copy(isResolving = true) }
        viewModelScope.launch {
            val result = runCatching {
                withContext(Dispatchers.Default) {
                    engine.playCard(before.game, cardId)
                }
            }
            _uiState.update { current ->
                result.fold(
                    onSuccess = { resolved ->
                        current.copy(
                            game = resolved,
                            learning = ai.summary(),
                            isResolving = false
                        )
                    },
                    onFailure = {
                        current.copy(
                            game = current.game.copy(
                                status =
                                    "Die neuronale Entscheidung konnte nicht abgeschlossen werden."
                            ),
                            isResolving = false
                        )
                    }
                )
            }
        }
    }

    fun continueRound() {
        if (_uiState.value.isResolving) return
        _uiState.update { it.copy(game = engine.continueAfterReveal(it.game)) }
    }

    fun newGame() {
        if (_uiState.value.isResolving || _uiState.value.isEvaluating) return
        _uiState.update { it.copy(game = engine.newGame(), learning = ai.summary()) }
    }

    fun setLearningPanel(visible: Boolean) {
        _uiState.update { it.copy(showLearningPanel = visible) }
    }

    fun trainAi(iterations: Int = 250) {
        if (_uiState.value.isResolving ||
            _uiState.value.isTraining ||
            _uiState.value.isEvaluating ||
            !ai.mode().trainable
        ) return
        _uiState.update {
            it.copy(
                isTraining = true,
                trainingProgress = 0,
                trainingTarget = iterations
            )
        }
        viewModelScope.launch {
            withContext(Dispatchers.Default) {
                ai.train(iterations) { progress ->
                    _uiState.update {
                        it.copy(
                            trainingProgress = progress,
                            learning = ai.summary()
                        )
                    }
                }
            }
            _uiState.update {
                it.copy(
                    isTraining = false,
                    trainingProgress = iterations,
                    learning = ai.summary()
                )
            }
        }
    }

    fun resetLearning() {
        if (_uiState.value.isResolving ||
            _uiState.value.isTraining ||
            _uiState.value.isEvaluating
        ) return
        ai.reset()
        _uiState.update {
            it.copy(
                learning = ai.summary(),
                trainingProgress = 0,
                trainingTarget = 0
            )
        }
    }

    fun setOpponentMode(mode: OpponentMode) {
        if (_uiState.value.isResolving ||
            _uiState.value.isTraining ||
            _uiState.value.isEvaluating
        ) return
        ai.setMode(mode)
        _uiState.update { it.copy(learning = ai.summary()) }
    }

    fun evaluateAi(gamesPerMode: Int = 5) {
        if (_uiState.value.isResolving ||
            _uiState.value.isTraining ||
            _uiState.value.isEvaluating
        ) return
        val modes = OpponentMode.entries.filter {
            it != OpponentMode.FROZEN_TATARUS
        }
        val target = gamesPerMode * modes.size
        _uiState.update {
            it.copy(
                isEvaluating = true,
                evaluationProgress = 0,
                evaluationTarget = target,
                evaluation = null
            )
        }
        viewModelScope.launch {
            val result = withContext(Dispatchers.Default) {
                ai.evaluate(
                    gamesPerMode = gamesPerMode,
                    modes = modes
                ) { progress, total ->
                    _uiState.update {
                        it.copy(
                            evaluationProgress = progress,
                            evaluationTarget = total
                        )
                    }
                }
            }
            _uiState.update {
                it.copy(
                    isEvaluating = false,
                    evaluationProgress = it.evaluationTarget,
                    evaluation = result,
                    learning = ai.summary()
                )
            }
        }
    }
}
