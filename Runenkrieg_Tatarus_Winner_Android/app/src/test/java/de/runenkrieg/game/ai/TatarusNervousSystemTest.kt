package de.runenkrieg.game.ai

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class TatarusNervousSystemTest {
    @Test
    fun largeScaleResearchProfileHasExpectedCapacity() {
        val nervousSystem = TatarusNervousSystem(seed = 1)
        val metrics = nervousSystem.metrics()

        assertEquals(1_024, metrics.neurons)
        assertEquals(128, metrics.inputChannels)
        assertEquals(1_024, metrics.inputProjections)
        assertEquals(32_768, metrics.synapses)
        assertTrue(metrics.stateBytesEstimate > 2L * 1024L * 1024L)
    }

    @Test
    fun everyInputChannelChangesTheNeuralState() {
        val baseline = TatarusNervousSystem(seed = 73)
        baseline.observe(
            DoubleArray(TatarusNervousSystem.INPUT_SIZE),
            duration = 1,
            plasticityWrite = false,
            updateAssemblies = false
        )
        val baselineHash = baseline.stateHash()

        repeat(TatarusNervousSystem.INPUT_SIZE) { channel ->
            val stimulated = TatarusNervousSystem(seed = 73)
            val input = DoubleArray(TatarusNervousSystem.INPUT_SIZE)
            input[channel] = 1.0
            stimulated.observe(
                input,
                duration = 1,
                plasticityWrite = false,
                updateAssemblies = false
            )

            assertNotEquals(
                "Eingabekanal $channel hat keinen neuronalen Einfluss.",
                baselineHash,
                stimulated.stateHash()
            )
        }
    }

    @Test
    fun sameSeedAndInputProduceTheSameNervousState() {
        val first = TatarusNervousSystem(seed = 17)
        val second = TatarusNervousSystem(seed = 17)
        val input = stimulus(phase = 0.15)

        val firstBridge = first.observe(input, duration = 24, plasticityWrite = true)
        val secondBridge = second.observe(input, duration = 24, plasticityWrite = true)

        assertArrayEquals(firstBridge, secondBridge, 0.0)
        assertEquals(first.stateHash(), second.stateHash())
    }

    @Test
    fun checkpointRestoresTheCompleteDynamicState() {
        val nervousSystem = TatarusNervousSystem(seed = 23)
        nervousSystem.observe(stimulus(phase = 0.25), 18, plasticityWrite = true)
        val checkpoint = nervousSystem.checkpoint()
        val expectedHash = nervousSystem.stateHash()

        nervousSystem.observe(stimulus(phase = 0.75), 31, plasticityWrite = true)
        assertNotEquals(expectedHash, nervousSystem.stateHash())

        nervousSystem.restore(checkpoint)
        assertEquals(expectedHash, nervousSystem.stateHash())
    }

    @Test
    fun rewardAndLongRunningActivityPreserveNumericAndDaleInvariants() {
        val nervousSystem = TatarusNervousSystem(seed = 41)

        repeat(20) { index ->
            nervousSystem.observe(
                stimulus(index / 20.0),
                duration = 12,
                plasticityWrite = true
            )
            nervousSystem.applyReward(if (index % 3 == 0) 0.8 else -0.35)
        }

        val metrics = nervousSystem.metrics()
        assertEquals(240L, metrics.steps)
        assertEquals(32_768, metrics.synapses)
        assertTrue("Der LargeScale-Kern erzeugt keine Spikes.", metrics.spikes > 0)
        assertTrue(
            "Der LargeScale-Kern überträgt keine rekurrenten Ereignisse.",
            metrics.transmissions > 0
        )
        assertTrue(metrics.assemblies in 1..64)
        assertTrue(metrics.meanEnergy in 0.0..1.0)
        assertTrue(nervousSystem.isFiniteAndDaleCompliant())
    }

    @Test
    fun distinctStimuliCreateSeparatedNonCollapsedAssemblies() {
        val nervousSystem = TatarusNervousSystem(seed = 91)

        repeat(12) { stimulusIndex ->
            val input = DoubleArray(TatarusNervousSystem.INPUT_SIZE)
            input[stimulusIndex] = 1.0
            input[64 + stimulusIndex % 10] = 1.0
            nervousSystem.observe(
                input,
                duration = 12,
                plasticityWrite = true
            )
        }

        val metrics = nervousSystem.metrics()
        assertTrue("Assembly-Bildung ist kollabiert.", metrics.assemblies >= 4)
        assertTrue("Assembly-Belegung besitzt keine Entropie.", metrics.assemblyEntropy > 0.25)
        assertTrue("Assembly-Prototypen sind nicht getrennt.", metrics.assemblySeparation > 0.25)
    }

    @Test
    fun extendedPlasticityAndEnergyMetricsRemainConsistent() {
        val nervousSystem = TatarusNervousSystem(seed = 107)
        repeat(30) { index ->
            nervousSystem.observe(
                stimulus(index / 30.0),
                duration = 10,
                plasticityWrite = true
            )
            nervousSystem.applyReward(if (index % 2 == 0) 0.7 else -0.6)
        }

        val metrics = nervousSystem.metrics()
        assertTrue(metrics.meanAbsoluteEligibility >= kotlin.math.abs(metrics.meanEligibility))
        assertTrue(metrics.maximumAbsoluteEligibility >= metrics.meanAbsoluteEligibility)
        assertTrue(metrics.activeEligibilityFraction in 0.0..1.0)
        assertTrue(metrics.saturatedEligibilityFraction in 0.0..1.0)
        assertTrue(metrics.positiveEligibilityFraction in 0.0..1.0)
        assertTrue(metrics.negativeEligibilityFraction in 0.0..1.0)
        assertTrue(metrics.minimumEnergy <= metrics.energyP10)
        assertTrue(metrics.energyP10 in 0.0..1.0)
        assertTrue(metrics.meanEnergy in 0.0..1.0)
        assertTrue(metrics.recentlyActiveSynapses in 0..metrics.synapses)
    }

    @Test
    fun assemblyAblationSuppressesAssemblyStateAndBridgeFeatures() {
        val nervousSystem = TatarusNervousSystem(seed = 113)
        val bridge = nervousSystem.observe(
            stimulus(0.4),
            duration = 24,
            plasticityWrite = true,
            updateAssemblies = false,
            exposeAssemblies = false
        )

        assertEquals(0, nervousSystem.metrics().assemblies)
        assertEquals(0.0, bridge[39], 0.0)
        assertEquals(0.0, bridge[40], 0.0)
        assertEquals(0.0, bridge[46], 0.0)
    }

    private fun stimulus(phase: Double): DoubleArray =
        DoubleArray(TatarusNervousSystem.INPUT_SIZE) { index ->
            val band = (index % 8) / 7.0
            ((band + phase) % 1.0).coerceIn(0.0, 1.0)
        }.also {
            it[30] = 1.0 - phase.coerceIn(0.0, 1.0)
            it[31] = 1.0
        }
}
