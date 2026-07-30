package de.runenkrieg.game.ai

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.ln
import kotlin.math.max
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tanh
import kotlin.random.Random

/**
 * Mobile game adapter of the TATARUS persistent synthetic nervous system.
 *
 * It preserves the mechanisms needed by an on-device opponent: recurrent
 * Dale-compliant E/I dynamics, a passive dendritic state, event-causal
 * generated release, individual delays, local signed eligibility, synaptic
 * resources, reward-bound consolidation, energy/homeostasis, competitive
 * assemblies and a bounded pooled bridge.
 *
 * This is intentionally smaller than the desktop research network. It is a
 * deterministic gameplay substrate, not a biological simulation.
 */
internal class TatarusNervousSystem(seed: Int = DEFAULT_SEED) {
    internal data class Metrics(
        val steps: Long,
        val spikes: Long,
        val transmissions: Long,
        val synapses: Int,
        val recentlyActiveSynapses: Int,
        val saturatedWeightFraction: Double,
        val assemblies: Int,
        val assemblyEntropy: Double,
        val assemblySeparation: Double,
        val assemblyReactivations: Long,
        val meanEnergy: Double,
        val minimumEnergy: Double,
        val energyP10: Double,
        val meanEligibility: Double,
        val meanAbsoluteEligibility: Double,
        val eligibilityStdDev: Double,
        val maximumAbsoluteEligibility: Double,
        val activeEligibilityFraction: Double,
        val saturatedEligibilityFraction: Double,
        val positiveEligibilityFraction: Double,
        val negativeEligibilityFraction: Double
    )

    internal data class DynamicSnapshot(
        val voltage: DoubleArray,
        val dendrite: DoubleArray,
        val adaptation: DoubleArray,
        val homeostasis: DoubleArray,
        val energy: DoubleArray,
        val fastRate: DoubleArray,
        val slowRate: DoubleArray,
        val spikeTrace: DoubleArray,
        val delayed: Array<DoubleArray>,
        val synapseState: Array<DoubleArray>,
        val assemblies: List<DoubleArray>,
        val assemblyHits: LongArray,
        val activeAssembly: Int,
        val novelty: Double,
        val assemblyReactivations: Long,
        val steps: Long,
        val spikes: Long,
        val transmissions: Long,
        val dopamine: Double
    )

    private data class Synapse(
        val pre: Int,
        val post: Int,
        val delay: Int,
        val inhibitory: Boolean,
        var weight: Double,
        var consolidatedWeight: Double,
        var eligibility: Double = 0.0,
        var resource: Double = 1.0,
        var facilitation: Double = 0.0,
        var usage: Double = 0.0
    )

    private val voltage = DoubleArray(NEURON_COUNT) { REST_MV }
    private val dendrite = DoubleArray(NEURON_COUNT) { REST_MV }
    private val adaptation = DoubleArray(NEURON_COUNT)
    private val homeostasis = DoubleArray(NEURON_COUNT)
    private val energy = DoubleArray(NEURON_COUNT) { 1.0 }
    private val fastRate = DoubleArray(NEURON_COUNT)
    private val slowRate = DoubleArray(NEURON_COUNT)
    private val spikeTrace = DoubleArray(NEURON_COUNT)
    private val delayed = Array(MAX_DELAY + 1) { DoubleArray(NEURON_COUNT) }
    private val synapses = mutableListOf<Synapse>()
    private val outgoing = Array(NEURON_COUNT) { mutableListOf<Int>() }
    private val assemblies = mutableListOf<DoubleArray>()
    private val assemblyHits = mutableListOf<Long>()

    private var activeAssembly = -1
    private var novelty = 1.0
    private var assemblyReactivations = 0L
    private var steps = 0L
    private var spikes = 0L
    private var transmissions = 0L
    private var dopamine = 0.0

    init {
        buildTopology(seed)
    }

    internal fun observe(
        input: DoubleArray,
        duration: Int,
        plasticityWrite: Boolean,
        updateAssemblies: Boolean = true,
        useEligibility: Boolean = true,
        useGeneratedOperator: Boolean = true,
        exposeAssemblies: Boolean = true
    ): DoubleArray {
        require(input.size == INPUT_SIZE)
        repeat(duration.coerceIn(1, 64)) {
            step(
                input = input,
                plasticityWrite = plasticityWrite,
                useEligibility = useEligibility,
                useGeneratedOperator = useGeneratedOperator
            )
        }
        if (updateAssemblies && exposeAssemblies) updateAssembly(input)
        return bridgeState(exposeAssemblies)
    }

    internal fun applyReward(reward: Double, useEligibility: Boolean = true) {
        val signal = reward.coerceIn(-1.0, 1.0)
        dopamine = (0.88 * dopamine + 0.12 * signal).coerceIn(-1.0, 1.0)
        if (!useEligibility) return
        synapses.forEach { synapse ->
            val delta = LEARNING_RATE * dopamine * synapse.eligibility
            val updated = synapse.weight + delta
            synapse.weight = if (synapse.inhibitory) {
                updated.coerceIn(-MAX_WEIGHT, -MIN_WEIGHT)
            } else {
                updated.coerceIn(MIN_WEIGHT, MAX_WEIGHT)
            }
            synapse.consolidatedWeight +=
                CONSOLIDATION_RATE * abs(dopamine * synapse.eligibility) *
                    (synapse.weight - synapse.consolidatedWeight)
        }
    }

    internal fun metrics(): Metrics {
        val eligibility = synapses.map { it.eligibility }
        val meanEligibility = eligibility.averageOrZero()
        val variance = eligibility.map {
            val delta = it - meanEligibility
            delta * delta
        }.averageOrZero()
        val sortedEnergy = energy.sorted()
        val energyP10Index =
            ((sortedEnergy.lastIndex * 0.10).toInt()).coerceIn(0, sortedEnergy.lastIndex)
        val totalAssemblyHits = assemblyHits.sum().toDouble()
        val assemblyEntropy = if (totalAssemblyHits <= 0.0 || assemblyHits.size <= 1) {
            0.0
        } else {
            val raw = assemblyHits.sumOf { hits ->
                val probability = hits / totalAssemblyHits
                if (probability <= 0.0) 0.0 else -probability * ln(probability)
            }
            raw / ln(assemblyHits.size.toDouble())
        }
        return Metrics(
            steps = steps,
            spikes = spikes,
            transmissions = transmissions,
            synapses = synapses.size,
            recentlyActiveSynapses = synapses.count { it.usage >= ACTIVE_USAGE_THRESHOLD },
            saturatedWeightFraction = synapses.count {
                abs(it.weight) >= MAX_WEIGHT * WEIGHT_SATURATION_FRACTION
            }.toDouble() / synapses.size,
            assemblies = assemblies.size,
            assemblyEntropy = assemblyEntropy,
            assemblySeparation = assemblySeparation(),
            assemblyReactivations = assemblyReactivations,
            meanEnergy = energy.average(),
            minimumEnergy = energy.minOrNull() ?: 1.0,
            energyP10 = sortedEnergy[energyP10Index],
            meanEligibility = meanEligibility,
            meanAbsoluteEligibility = eligibility.map(::abs).averageOrZero(),
            eligibilityStdDev = sqrt(variance),
            maximumAbsoluteEligibility = eligibility.maxOfOrNull(::abs) ?: 0.0,
            activeEligibilityFraction = eligibility.count {
                abs(it) >= ACTIVE_ELIGIBILITY_THRESHOLD
            }.toDouble() / eligibility.size,
            saturatedEligibilityFraction = eligibility.count {
                abs(it) >= ELIGIBILITY_MAX * ELIGIBILITY_SATURATION_FRACTION
            }.toDouble() / eligibility.size,
            positiveEligibilityFraction = eligibility.count {
                it >= ACTIVE_ELIGIBILITY_THRESHOLD
            }.toDouble() / eligibility.size,
            negativeEligibilityFraction = eligibility.count {
                it <= -ACTIVE_ELIGIBILITY_THRESHOLD
            }.toDouble() / eligibility.size
        )
    }

    internal fun checkpoint(): DynamicSnapshot = DynamicSnapshot(
        voltage = voltage.copyOf(),
        dendrite = dendrite.copyOf(),
        adaptation = adaptation.copyOf(),
        homeostasis = homeostasis.copyOf(),
        energy = energy.copyOf(),
        fastRate = fastRate.copyOf(),
        slowRate = slowRate.copyOf(),
        spikeTrace = spikeTrace.copyOf(),
        delayed = Array(delayed.size) { delayed[it].copyOf() },
        synapseState = synapses.map {
            doubleArrayOf(
                it.weight,
                it.consolidatedWeight,
                it.eligibility,
                it.resource,
                it.facilitation,
                it.usage
            )
        }.toTypedArray(),
        assemblies = assemblies.map { it.copyOf() },
        assemblyHits = assemblyHits.toLongArray(),
        activeAssembly = activeAssembly,
        novelty = novelty,
        assemblyReactivations = assemblyReactivations,
        steps = steps,
        spikes = spikes,
        transmissions = transmissions,
        dopamine = dopamine
    )

    internal fun restore(snapshot: DynamicSnapshot) {
        snapshot.voltage.copyInto(voltage)
        snapshot.dendrite.copyInto(dendrite)
        snapshot.adaptation.copyInto(adaptation)
        snapshot.homeostasis.copyInto(homeostasis)
        snapshot.energy.copyInto(energy)
        snapshot.fastRate.copyInto(fastRate)
        snapshot.slowRate.copyInto(slowRate)
        snapshot.spikeTrace.copyInto(spikeTrace)
        delayed.indices.forEach { snapshot.delayed[it].copyInto(delayed[it]) }
        synapses.indices.forEach { index ->
            val values = snapshot.synapseState[index]
            synapses[index].apply {
                weight = values[0]
                consolidatedWeight = values[1]
                eligibility = values[2]
                resource = values[3]
                facilitation = values[4]
                usage = values[5]
            }
        }
        assemblies.clear()
        assemblies += snapshot.assemblies.map { it.copyOf() }
        assemblyHits.clear()
        assemblyHits += snapshot.assemblyHits.toList()
        activeAssembly = snapshot.activeAssembly
        novelty = snapshot.novelty
        assemblyReactivations = snapshot.assemblyReactivations
        steps = snapshot.steps
        spikes = snapshot.spikes
        transmissions = snapshot.transmissions
        dopamine = snapshot.dopamine
    }

    internal fun stateHash(): Long {
        var hash = -3750763034362895579L
        fun add(value: Long) {
            hash = (hash xor value) * 1099511628211L
        }
        listOf(
            voltage,
            dendrite,
            adaptation,
            homeostasis,
            energy,
            fastRate,
            slowRate,
            spikeTrace
        ).forEach { values -> values.forEach { add(it.toBits()) } }
        delayed.forEach { values -> values.forEach { add(it.toBits()) } }
        synapses.forEach {
            add(it.weight.toBits())
            add(it.consolidatedWeight.toBits())
            add(it.eligibility.toBits())
            add(it.resource.toBits())
            add(it.facilitation.toBits())
            add(it.usage.toBits())
        }
        assemblies.forEach { prototype -> prototype.forEach { add(it.toBits()) } }
        assemblyHits.forEach(::add)
        add(activeAssembly.toLong())
        add(novelty.toBits())
        add(assemblyReactivations)
        add(steps)
        add(spikes)
        add(transmissions)
        add(dopamine.toBits())
        return hash
    }

    internal fun isFiniteAndDaleCompliant(): Boolean =
        voltage.all(Double::isFinite) &&
            dendrite.all(Double::isFinite) &&
            energy.all { it.isFinite() && it in 0.0..1.0 } &&
            synapses.all {
                it.weight.isFinite() &&
                    it.eligibility.isFinite() &&
                    if (it.inhibitory) it.weight < 0.0 else it.weight > 0.0
            }

    internal fun toJson(): JSONObject {
        val root = JSONObject()
            .put("schema", SNAPSHOT_SCHEMA)
            .put("steps", steps)
            .put("spikes", spikes)
            .put("transmissions", transmissions)
            .put("activeAssembly", activeAssembly)
            .put("novelty", novelty)
            .put("assemblyReactivations", assemblyReactivations)
            .put("dopamine", dopamine)
            .put("voltage", voltage.toJson())
            .put("dendrite", dendrite.toJson())
            .put("adaptation", adaptation.toJson())
            .put("homeostasis", homeostasis.toJson())
            .put("energy", energy.toJson())
            .put("fastRate", fastRate.toJson())
            .put("slowRate", slowRate.toJson())
            .put("spikeTrace", spikeTrace.toJson())

        val delayJson = JSONArray()
        delayed.forEach { delayJson.put(it.toJson()) }
        root.put("delayed", delayJson)

        val synapseJson = JSONArray()
        synapses.forEach {
            synapseJson.put(
                JSONArray()
                    .put(it.weight)
                    .put(it.consolidatedWeight)
                    .put(it.eligibility)
                    .put(it.resource)
                    .put(it.facilitation)
                    .put(it.usage)
            )
        }
        root.put("synapses", synapseJson)

        val assemblyJson = JSONArray()
        assemblies.forEach { assemblyJson.put(it.toJson()) }
        root.put("assemblies", assemblyJson)
        val assemblyHitsJson = JSONArray()
        assemblyHits.forEach { assemblyHitsJson.put(it) }
        root.put("assemblyHits", assemblyHitsJson)
        return root
    }

    internal fun loadJson(root: JSONObject) {
        require(root.optInt("schema") == SNAPSHOT_SCHEMA)
        root.getJSONArray("voltage").copyInto(voltage)
        root.getJSONArray("dendrite").copyInto(dendrite)
        root.getJSONArray("adaptation").copyInto(adaptation)
        root.getJSONArray("homeostasis").copyInto(homeostasis)
        root.getJSONArray("energy").copyInto(energy)
        root.getJSONArray("fastRate").copyInto(fastRate)
        root.getJSONArray("slowRate").copyInto(slowRate)
        root.getJSONArray("spikeTrace").copyInto(spikeTrace)

        val delayJson = root.getJSONArray("delayed")
        require(delayJson.length() == delayed.size)
        delayed.indices.forEach { delayJson.getJSONArray(it).copyInto(delayed[it]) }

        val synapseJson = root.getJSONArray("synapses")
        require(synapseJson.length() == synapses.size)
        synapses.indices.forEach { index ->
            val values = synapseJson.getJSONArray(index)
            synapses[index].apply {
                weight = values.getDouble(0)
                consolidatedWeight = values.getDouble(1)
                eligibility = values.getDouble(2)
                resource = values.getDouble(3)
                facilitation = values.getDouble(4)
                usage = values.getDouble(5)
            }
        }

        assemblies.clear()
        val assemblyJson = root.optJSONArray("assemblies") ?: JSONArray()
        repeat(assemblyJson.length()) { index ->
            assemblies += assemblyJson.getJSONArray(index).toDoubleArray()
        }
        assemblyHits.clear()
        val assemblyHitsJson = root.optJSONArray("assemblyHits") ?: JSONArray()
        repeat(assemblyHitsJson.length()) { index ->
            assemblyHits += assemblyHitsJson.getLong(index)
        }
        require(assemblyHits.size == assemblies.size)
        steps = root.optLong("steps")
        spikes = root.optLong("spikes")
        transmissions = root.optLong("transmissions")
        activeAssembly = root.optInt("activeAssembly", -1)
        novelty = root.optDouble("novelty", 1.0)
        assemblyReactivations = root.optLong("assemblyReactivations")
        dopamine = root.optDouble("dopamine", 0.0)
        require(isFiniteAndDaleCompliant())
    }

    private fun buildTopology(seed: Int) {
        val random = Random(seed)
        repeat(NEURON_COUNT) { pre ->
            val targets = mutableSetOf<Int>()
            while (targets.size < OUT_DEGREE) {
                val target = random.nextInt(NEURON_COUNT)
                if (target != pre) targets += target
            }
            targets.sorted().forEach { post ->
                val inhibitory = pre in INHIBITORY_RANGE
                val magnitude = if (inhibitory) {
                    4.0 + random.nextDouble() * 3.0
                } else {
                    2.5 + random.nextDouble() * 3.0
                }
                val weight = if (inhibitory) -magnitude else magnitude
                val synapse = Synapse(
                    pre = pre,
                    post = post,
                    delay = random.nextInt(1, MAX_DELAY + 1),
                    inhibitory = inhibitory,
                    weight = weight,
                    consolidatedWeight = weight
                )
                outgoing[pre] += synapses.size
                synapses += synapse
            }
        }
    }

    private fun step(
        input: DoubleArray,
        plasticityWrite: Boolean,
        useEligibility: Boolean,
        useGeneratedOperator: Boolean
    ) {
        val slot = (steps % delayed.size).toInt()
        val recurrent = delayed[slot].copyOf()
        delayed[slot].fill(0.0)

        var excitatoryDrive = input.sumOf { max(0.0, it) } + 1e-9
        var inhibitoryDrive = 1e-9
        recurrent.forEach {
            if (it >= 0.0) excitatoryDrive += it else inhibitoryDrive += -it
        }
        val balance =
            ((excitatoryDrive - inhibitoryDrive) /
                (excitatoryDrive + inhibitoryDrive)).coerceIn(-1.0, 1.0)

        val fired = BooleanArray(NEURON_COUNT)
        repeat(NEURON_COUNT) { neuron ->
            val external = externalDrive(neuron, input)
            dendrite[neuron] +=
                ((REST_MV - dendrite[neuron]) + recurrent[neuron] + external) /
                    TAU_DENDRITE_MS
            voltage[neuron] +=
                ((REST_MV - voltage[neuron]) +
                    SOMA_DENDRITE_COUPLING * (dendrite[neuron] - voltage[neuron]) +
                    BASE_CURRENT) / TAU_SOMA_MS

            val threshold = THRESHOLD_MV + adaptation[neuron] + homeostasis[neuron]
            if (voltage[neuron] >= threshold && energy[neuron] >= SPIKE_ENERGY_COST) {
                fired[neuron] = true
                voltage[neuron] = RESET_MV
                adaptation[neuron] += ADAPTATION_INCREMENT_MV
                energy[neuron] = (energy[neuron] - SPIKE_ENERGY_COST).coerceAtLeast(0.0)
                spikes += 1
            }
            adaptation[neuron] *= ADAPTATION_DECAY
            fastRate[neuron] = fastRate[neuron] * 0.88 + if (fired[neuron]) 0.12 else 0.0
            slowRate[neuron] = slowRate[neuron] * 0.995 + if (fired[neuron]) 0.005 else 0.0
            homeostasis[neuron] = (
                homeostasis[neuron] +
                    HOMEOSTASIS_GAIN * (fastRate[neuron] * 1000.0 - TARGET_RATE_HZ)
                ).coerceIn(-8.0, 8.0)
            energy[neuron] = (energy[neuron] + ENERGY_RECOVERY).coerceAtMost(1.0)
        }

        val eligibilityDecay = exp(-1.0 / ELIGIBILITY_TAU_MS)
        synapses.forEach { synapse ->
            synapse.eligibility *= eligibilityDecay
            if (plasticityWrite && useEligibility) {
                val causal =
                    (if (fired[synapse.post]) spikeTrace[synapse.pre] else 0.0) -
                        (if (fired[synapse.pre]) spikeTrace[synapse.post] else 0.0)
                synapse.eligibility =
                    (synapse.eligibility + ELIGIBILITY_INCREMENT * causal)
                        .coerceIn(-ELIGIBILITY_MAX, ELIGIBILITY_MAX)
            }
            synapse.resource =
                (synapse.resource + (1.0 - synapse.resource) / RESOURCE_TAU_MS)
                    .coerceIn(0.0, 1.0)
            synapse.facilitation *= FACILITATION_DECAY
            synapse.usage *= 0.999
        }

        repeat(NEURON_COUNT) { pre ->
            if (!fired[pre]) return@repeat
            outgoing[pre].forEach { synapseIndex ->
                val synapse = synapses[synapseIndex]
                val releaseProbability =
                    (BASE_RELEASE + synapse.facilitation).coerceIn(0.02, 0.95)
                val gate = if (useGeneratedOperator) {
                    generatedReleaseGate(
                        (balance + 0.2 * (spikeTrace[pre] - spikeTrace[synapse.post]))
                            .coerceIn(-1.0, 1.0)
                    )
                } else {
                    CONSTANT_CONTROL_GATE
                }
                val eligibilityModulation = if (useEligibility) {
                    (1.0 + ELIGIBILITY_TRANSMISSION_GAIN * tanh(synapse.eligibility))
                        .coerceIn(0.25, 2.0)
                } else {
                    1.0
                }
                val amplitude =
                    synapse.weight * releaseProbability * synapse.resource *
                        gate * eligibilityModulation
                val delivery = ((steps + synapse.delay) % delayed.size).toInt()
                delayed[delivery][synapse.post] += amplitude
                synapse.resource =
                    (synapse.resource * (1.0 - releaseProbability * 0.35))
                        .coerceIn(0.0, 1.0)
                synapse.facilitation =
                    (synapse.facilitation + 0.12 * (1.0 - synapse.facilitation))
                        .coerceIn(0.0, 0.8)
                synapse.usage += abs(amplitude)
                energy[pre] =
                    (energy[pre] - TRANSMISSION_ENERGY_COST).coerceAtLeast(0.0)
                transmissions += 1
            }
        }

        repeat(NEURON_COUNT) { neuron ->
            spikeTrace[neuron] =
                spikeTrace[neuron] * TRACE_DECAY + if (fired[neuron]) 1.0 else 0.0
        }
        steps += 1
    }

    private fun externalDrive(neuron: Int, input: DoubleArray): Double = when (neuron) {
        in SENSORY_RANGE ->
            SENSORY_DRIVE * input[neuron - SENSORY_RANGE.first]
        in AUXILIARY_INPUT_RANGE ->
            CONTEXT_DRIVE *
                input[AUXILIARY_INPUT_OFFSET + neuron - AUXILIARY_INPUT_RANGE.first]
        in CONTEXT_RANGE ->
            CONTEXT_DRIVE *
                input[CONTEXT_INPUT_OFFSET + neuron - CONTEXT_RANGE.first]
        else -> 0.0
    }

    private fun updateAssembly(input: DoubleArray) {
        val inputMean = input.average()
        val pattern = DoubleArray(ASSEMBLY_FEATURES) { index ->
            val neuron = inputNeuron(index)
            val neuralState = tanh(
                fastRate[neuron] - slowRate[neuron] +
                    (dendrite[neuron] - REST_MV) / 20.0
            )
            ASSEMBLY_INPUT_WEIGHT * (input[index] - inputMean) +
                ASSEMBLY_NEURAL_WEIGHT * neuralState
        }
        normalizeCentered(pattern)
        var bestIndex = -1
        var bestSimilarity = -1.0
        var bestDistance = Double.POSITIVE_INFINITY
        assemblies.forEachIndexed { index, prototype ->
            val similarity = cosineSimilarity(prototype, pattern)
            val distance = euclideanDistance(prototype, pattern)
            if (similarity > bestSimilarity ||
                (similarity == bestSimilarity && distance < bestDistance)
            ) {
                bestSimilarity = similarity
                bestDistance = distance
                bestIndex = index
            }
        }
        val matches =
            bestIndex >= 0 &&
                bestSimilarity >= ASSEMBLY_SIMILARITY_THRESHOLD &&
                bestDistance <= ASSEMBLY_DISTANCE_THRESHOLD
        if (bestIndex < 0 || (!matches && assemblies.size < MAX_ASSEMBLIES)) {
            assemblies += pattern
            assemblyHits += 1L
            activeAssembly = assemblies.lastIndex
            novelty = 1.0
        } else {
            val prototype = assemblies[bestIndex]
            if (matches) {
                prototype.indices.forEach { index ->
                    prototype[index] +=
                        ASSEMBLY_LEARNING_RATE * (pattern[index] - prototype[index])
                }
                normalizeCentered(prototype)
                assemblyReactivations += 1
            }
            activeAssembly = bestIndex
            assemblyHits[bestIndex] += 1
            novelty = max(
                (1.0 - bestSimilarity).coerceIn(0.0, 1.0),
                (bestDistance / 2.0).coerceIn(0.0, 1.0)
            )
        }
    }

    private fun bridgeState(exposeAssemblies: Boolean): DoubleArray {
        val result = DoubleArray(BRIDGE_SIZE)
        repeat(MOTOR_RANGE.count()) { index ->
            result[index] = fastRate[MOTOR_RANGE.first + index].coerceIn(0.0, 1.0)
        }
        result[4] = SENSORY_RANGE.map { fastRate[it] }.average()
        result[5] = EXCITATORY_RANGE.map { fastRate[it] }.average()
        result[6] = INHIBITORY_RANGE.map { fastRate[it] }.average()
        result[7] = CONTEXT_RANGE.map { fastRate[it] }.average()
        result[8] = energy.average()
        result[9] = tanh(synapses.map { it.eligibility }.averageOrZero())
        result[10] = synapses.map { it.resource }.averageOrZero()
        result[11] = if (!exposeAssemblies || activeAssembly < 0) {
            0.0
        } else {
            activeAssembly.toDouble() / max(1, MAX_ASSEMBLIES - 1)
        }
        result[12] = if (exposeAssemblies) novelty else 0.0
        result[13] = (TARGET_RATE_HZ / 1000.0 -
            EXCITATORY_RANGE.map { fastRate[it] }.average()).coerceIn(-1.0, 1.0)
        result[14] = tanh(dopamine)
        result[15] = 1.0
        return result
    }

    private fun inputNeuron(channel: Int): Int = when (channel) {
        in 0..23 -> SENSORY_RANGE.first + channel
        in 24..27 -> AUXILIARY_INPUT_RANGE.first + channel - 24
        else -> CONTEXT_RANGE.first + channel - 28
    }

    private fun normalizeCentered(values: DoubleArray) {
        val mean = values.average()
        var normSquared = 0.0
        values.indices.forEach { index ->
            values[index] -= mean
            normSquared += values[index] * values[index]
        }
        val norm = sqrt(normSquared)
        if (norm > 1e-9) {
            values.indices.forEach { index -> values[index] /= norm }
        }
    }

    private fun euclideanDistance(first: DoubleArray, second: DoubleArray): Double {
        var squared = 0.0
        first.indices.forEach { index ->
            val delta = first[index] - second[index]
            squared += delta * delta
        }
        return sqrt(squared)
    }

    private fun assemblySeparation(): Double {
        if (assemblies.size < 2) return 0.0
        var distance = 0.0
        var pairs = 0
        for (first in 0 until assemblies.lastIndex) {
            for (second in first + 1 until assemblies.size) {
                distance += euclideanDistance(assemblies[first], assemblies[second])
                pairs += 1
            }
        }
        return if (pairs == 0) 0.0 else distance / pairs
    }

    private fun generatedReleaseGate(phi: Double): Double =
        ((1.0 + tanh(generatedKernel(phi))) * 0.5).coerceIn(0.05, 0.95)

    /**
     * Exact scalar Algorithmic-Genesis kernel port from the TATARUS export.
     */
    private fun generatedKernel(value: Double): Double {
        val x = sanitize(value)
        return sanitize(
            (
                logAbs(sin(cos(safeDivide(x, x)))) -
                    tanh(
                        safeDivide(
                            sin(safeDivide(x, x)) * -0.357064,
                            logAbs(sin(cos(safeDivide(x, x)))) -
                                logAbs(sin(cos(safeDivide(x, x))))
                        )
                    )
                ) -
                sin(
                    logAbs(sin(cos(safeDivide(x, x)))) *
                        (
                            sin(cos(safeDivide(x, x))) +
                                cos(sin(safeDivide(x, x)))
                            )
                )
        )
    }

    private fun sanitize(value: Double): Double =
        if (value.isFinite()) value.coerceIn(-1_000_000.0, 1_000_000.0) else 0.0

    private fun safeDivide(a: Double, b: Double): Double =
        sanitize(a) / (abs(sanitize(b)) + 1e-6)

    private fun logAbs(value: Double): Double = ln(abs(sanitize(value)) + 1e-9)

    private fun cosineSimilarity(first: DoubleArray, second: DoubleArray): Double {
        var dot = 0.0
        var firstNorm = 0.0
        var secondNorm = 0.0
        first.indices.forEach { index ->
            dot += first[index] * second[index]
            firstNorm += first[index] * first[index]
            secondNorm += second[index] * second[index]
        }
        return dot / (sqrt(firstNorm * secondNorm) + 1e-9)
    }

    private fun DoubleArray.toJson(): JSONArray =
        JSONArray().also { array -> forEach { value -> array.put(value) } }

    private fun JSONArray.toDoubleArray(): DoubleArray =
        DoubleArray(length()) { getDouble(it) }

    private fun JSONArray.copyInto(destination: DoubleArray) {
        require(length() == destination.size)
        destination.indices.forEach { destination[it] = getDouble(it) }
    }

    private fun List<Double>.averageOrZero(): Double =
        if (isEmpty()) 0.0 else average()

    companion object {
        const val INPUT_SIZE = 32
        const val BRIDGE_SIZE = 16
        const val DEFAULT_SEED = 0x54415441

        private const val SNAPSHOT_SCHEMA = 2
        private const val NEURON_COUNT = 72
        private const val OUT_DEGREE = 6
        private const val MAX_DELAY = 5
        private const val ASSEMBLY_FEATURES = 32
        private const val MAX_ASSEMBLIES = 16

        private val SENSORY_RANGE = 0..23
        private val AUXILIARY_INPUT_RANGE = 24..27
        private val EXCITATORY_RANGE = 24..51
        private val INHIBITORY_RANGE = 52..63
        private val CONTEXT_RANGE = 64..67
        private val MOTOR_RANGE = 68..71
        private const val AUXILIARY_INPUT_OFFSET = 24
        private const val CONTEXT_INPUT_OFFSET = 28
        private const val SENSORY_DRIVE = 26.0
        private const val CONTEXT_DRIVE = 16.0

        private const val REST_MV = -65.0
        private const val RESET_MV = -70.0
        private const val THRESHOLD_MV = -50.0
        private const val TAU_SOMA_MS = 20.0
        private const val TAU_DENDRITE_MS = 35.0
        private const val SOMA_DENDRITE_COUPLING = 0.22
        private const val BASE_CURRENT = 12.5
        private const val ADAPTATION_INCREMENT_MV = 1.2
        private const val ADAPTATION_DECAY = 0.99
        private const val TARGET_RATE_HZ = 8.0
        private const val HOMEOSTASIS_GAIN = 0.00003

        private const val ELIGIBILITY_TAU_MS = 400.0
        private const val ELIGIBILITY_INCREMENT = 0.35
        private const val ELIGIBILITY_MAX = 4.0
        private const val ELIGIBILITY_TRANSMISSION_GAIN = 0.5
        private const val RESOURCE_TAU_MS = 180.0
        private const val FACILITATION_DECAY = 0.9917
        private const val BASE_RELEASE = 0.18
        private const val TRACE_DECAY = 0.95
        private const val CONSTANT_CONTROL_GATE = 0.5

        private const val LEARNING_RATE = 0.006
        private const val CONSOLIDATION_RATE = 0.002
        private const val MIN_WEIGHT = 0.05
        private const val MAX_WEIGHT = 8.0
        private const val SPIKE_ENERGY_COST = 0.025
        private const val TRANSMISSION_ENERGY_COST = 0.0004
        private const val ENERGY_RECOVERY = 0.0015
        private const val ASSEMBLY_SIMILARITY_THRESHOLD = 0.78
        private const val ASSEMBLY_DISTANCE_THRESHOLD = 0.70
        private const val ASSEMBLY_LEARNING_RATE = 0.12
        private const val ASSEMBLY_INPUT_WEIGHT = 0.55
        private const val ASSEMBLY_NEURAL_WEIGHT = 0.45
        private const val ACTIVE_USAGE_THRESHOLD = 0.10
        private const val ACTIVE_ELIGIBILITY_THRESHOLD = 0.05
        private const val ELIGIBILITY_SATURATION_FRACTION = 0.95
        private const val WEIGHT_SATURATION_FRACTION = 0.95
    }
}
