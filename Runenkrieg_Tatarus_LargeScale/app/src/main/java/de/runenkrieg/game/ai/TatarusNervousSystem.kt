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
 * This LargeScale branch deliberately expands the mobile research substrate.
 * It remains a deterministic synthetic nervous system, not a claim of a
 * biologically complete nervous-system simulation.
 */
internal class TatarusNervousSystem(seed: Int = DEFAULT_SEED) {
    internal data class Metrics(
        val steps: Long,
        val spikes: Long,
        val transmissions: Long,
        val neurons: Int,
        val inputChannels: Int,
        val inputProjections: Int,
        val synapses: Int,
        val stateBytesEstimate: Long,
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
        val synapseWeight: DoubleArray,
        val synapseConsolidatedWeight: DoubleArray,
        val synapseEligibility: DoubleArray,
        val synapseResource: DoubleArray,
        val synapseFacilitation: DoubleArray,
        val synapseUsage: DoubleArray,
        val assemblies: List<DoubleArray>,
        val assemblyHits: LongArray,
        val activeAssembly: Int,
        val novelty: Double,
        val assemblyReactivations: Long,
        val steps: Long,
        val spikes: Long,
        val transmissions: Long,
        val dopamine: Double,
        val lastBalance: Double
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
    private val inputTargets = Array(INPUT_SIZE) { IntArray(INPUT_FANOUT) }
    private val externalCurrent = DoubleArray(NEURON_COUNT)
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
    private var lastBalance = 0.0

    init {
        buildInputProjections()
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
        var eligibilitySum = 0.0
        var eligibilitySquaredSum = 0.0
        var absoluteEligibilitySum = 0.0
        var maximumAbsoluteEligibility = 0.0
        var activeEligibility = 0
        var saturatedEligibility = 0
        var positiveEligibility = 0
        var negativeEligibility = 0
        var recentlyActive = 0
        var saturatedWeights = 0
        synapses.forEach { synapse ->
            val eligibility = synapse.eligibility
            val absoluteEligibility = abs(eligibility)
            eligibilitySum += eligibility
            eligibilitySquaredSum += eligibility * eligibility
            absoluteEligibilitySum += absoluteEligibility
            maximumAbsoluteEligibility = max(maximumAbsoluteEligibility, absoluteEligibility)
            if (absoluteEligibility >= ACTIVE_ELIGIBILITY_THRESHOLD) activeEligibility += 1
            if (absoluteEligibility >=
                ELIGIBILITY_MAX * ELIGIBILITY_SATURATION_FRACTION
            ) {
                saturatedEligibility += 1
            }
            if (eligibility >= ACTIVE_ELIGIBILITY_THRESHOLD) positiveEligibility += 1
            if (eligibility <= -ACTIVE_ELIGIBILITY_THRESHOLD) negativeEligibility += 1
            if (synapse.usage >= ACTIVE_USAGE_THRESHOLD) recentlyActive += 1
            if (abs(synapse.weight) >=
                MAX_WEIGHT * WEIGHT_SATURATION_FRACTION
            ) {
                saturatedWeights += 1
            }
        }
        val synapseCount = synapses.size.coerceAtLeast(1)
        val meanEligibility = eligibilitySum / synapseCount
        val variance =
            (eligibilitySquaredSum / synapseCount - meanEligibility * meanEligibility)
                .coerceAtLeast(0.0)
        val sortedEnergy = energy.sortedArray()
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
            neurons = NEURON_COUNT,
            inputChannels = INPUT_SIZE,
            inputProjections = INPUT_PROJECTION_COUNT,
            synapses = synapses.size,
            stateBytesEstimate = stateBytesEstimate(),
            recentlyActiveSynapses = recentlyActive,
            saturatedWeightFraction = saturatedWeights.toDouble() / synapseCount,
            assemblies = assemblies.size,
            assemblyEntropy = assemblyEntropy,
            assemblySeparation = assemblySeparation(),
            assemblyReactivations = assemblyReactivations,
            meanEnergy = energy.average(),
            minimumEnergy = energy.minOrNull() ?: 1.0,
            energyP10 = sortedEnergy[energyP10Index],
            meanEligibility = meanEligibility,
            meanAbsoluteEligibility = absoluteEligibilitySum / synapseCount,
            eligibilityStdDev = sqrt(variance),
            maximumAbsoluteEligibility = maximumAbsoluteEligibility,
            activeEligibilityFraction = activeEligibility.toDouble() / synapseCount,
            saturatedEligibilityFraction = saturatedEligibility.toDouble() / synapseCount,
            positiveEligibilityFraction = positiveEligibility.toDouble() / synapseCount,
            negativeEligibilityFraction = negativeEligibility.toDouble() / synapseCount
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
        synapseWeight = DoubleArray(synapses.size) { synapses[it].weight },
        synapseConsolidatedWeight =
            DoubleArray(synapses.size) { synapses[it].consolidatedWeight },
        synapseEligibility = DoubleArray(synapses.size) { synapses[it].eligibility },
        synapseResource = DoubleArray(synapses.size) { synapses[it].resource },
        synapseFacilitation = DoubleArray(synapses.size) { synapses[it].facilitation },
        synapseUsage = DoubleArray(synapses.size) { synapses[it].usage },
        assemblies = assemblies.map { it.copyOf() },
        assemblyHits = assemblyHits.toLongArray(),
        activeAssembly = activeAssembly,
        novelty = novelty,
        assemblyReactivations = assemblyReactivations,
        steps = steps,
        spikes = spikes,
        transmissions = transmissions,
        dopamine = dopamine,
        lastBalance = lastBalance
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
        require(snapshot.synapseWeight.size == synapses.size)
        synapses.indices.forEach { index ->
            synapses[index].apply {
                weight = snapshot.synapseWeight[index]
                consolidatedWeight = snapshot.synapseConsolidatedWeight[index]
                eligibility = snapshot.synapseEligibility[index]
                resource = snapshot.synapseResource[index]
                facilitation = snapshot.synapseFacilitation[index]
                usage = snapshot.synapseUsage[index]
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
        lastBalance = snapshot.lastBalance
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
        add(lastBalance.toBits())
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
            .put("lastBalance", lastBalance)
            .put("neurons", NEURON_COUNT)
            .put("inputChannels", INPUT_SIZE)
            .put("inputProjections", INPUT_PROJECTION_COUNT)
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

        root
            .put(
                "synapseWeight",
                DoubleArray(synapses.size) { synapses[it].weight }.toJson()
            )
            .put(
                "synapseConsolidatedWeight",
                DoubleArray(synapses.size) {
                    synapses[it].consolidatedWeight
                }.toJson()
            )
            .put(
                "synapseEligibility",
                DoubleArray(synapses.size) { synapses[it].eligibility }.toJson()
            )
            .put(
                "synapseResource",
                DoubleArray(synapses.size) { synapses[it].resource }.toJson()
            )
            .put(
                "synapseFacilitation",
                DoubleArray(synapses.size) { synapses[it].facilitation }.toJson()
            )
            .put(
                "synapseUsage",
                DoubleArray(synapses.size) { synapses[it].usage }.toJson()
            )

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

        val synapseWeight = root.getJSONArray("synapseWeight")
        val synapseConsolidatedWeight =
            root.getJSONArray("synapseConsolidatedWeight")
        val synapseEligibility = root.getJSONArray("synapseEligibility")
        val synapseResource = root.getJSONArray("synapseResource")
        val synapseFacilitation = root.getJSONArray("synapseFacilitation")
        val synapseUsage = root.getJSONArray("synapseUsage")
        listOf(
            synapseWeight,
            synapseConsolidatedWeight,
            synapseEligibility,
            synapseResource,
            synapseFacilitation,
            synapseUsage
        ).forEach { require(it.length() == synapses.size) }
        synapses.indices.forEach { index ->
            synapses[index].apply {
                weight = synapseWeight.getDouble(index)
                consolidatedWeight = synapseConsolidatedWeight.getDouble(index)
                eligibility = synapseEligibility.getDouble(index)
                resource = synapseResource.getDouble(index)
                facilitation = synapseFacilitation.getDouble(index)
                usage = synapseUsage.getDouble(index)
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
        lastBalance = root.optDouble("lastBalance", 0.0)
        require(isFiniteAndDaleCompliant())
    }

    private fun buildInputProjections() {
        repeat(INPUT_SIZE) { channel ->
            inputTargets[channel][0] = INPUT_RANGE.first + channel
            val usedTargets = mutableSetOf(inputTargets[channel][0])
            repeat(INPUT_FANOUT - 1) { fanoutIndex ->
                val fanout = fanoutIndex + 1
                var target = if (fanout <= EXCITATORY_FANOUT) {
                    EXCITATORY_RANGE.first +
                        ((channel * 73 + fanout * 97) % EXCITATORY_RANGE.count())
                } else {
                    CONTEXT_RANGE.first +
                        ((channel * 17 + fanout * 13) % CONTEXT_RANGE.count())
                }
                while (!usedTargets.add(target)) {
                    target = if (target in EXCITATORY_RANGE) {
                        EXCITATORY_RANGE.first +
                            ((target - EXCITATORY_RANGE.first + 1) %
                                EXCITATORY_RANGE.count())
                    } else {
                        CONTEXT_RANGE.first +
                            ((target - CONTEXT_RANGE.first + 1) % CONTEXT_RANGE.count())
                    }
                }
                inputTargets[channel][fanout] = target
            }
        }
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
                    INHIBITORY_WEIGHT_MIN +
                        random.nextDouble() * INHIBITORY_WEIGHT_SPAN
                } else {
                    EXCITATORY_WEIGHT_MIN +
                        random.nextDouble() * EXCITATORY_WEIGHT_SPAN
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

        externalCurrent.fill(0.0)
        repeat(INPUT_SIZE) { channel ->
            val value = input[channel]
            if (value == 0.0) return@repeat
            inputTargets[channel].forEachIndexed { fanout, target ->
                val drive = if (fanout == 0) INPUT_DIRECT_DRIVE else INPUT_FANOUT_DRIVE
                externalCurrent[target] += value * drive
            }
        }

        var excitatoryDrive = externalCurrent.sumOf { max(0.0, it) } + 1e-9
        var inhibitoryDrive = 1e-9
        recurrent.forEach {
            if (it >= 0.0) excitatoryDrive += it else inhibitoryDrive += -it
        }
        val balance =
            ((excitatoryDrive - inhibitoryDrive) /
                (excitatoryDrive + inhibitoryDrive)).coerceIn(-1.0, 1.0)
        lastBalance = balance

        val fired = BooleanArray(NEURON_COUNT)
        repeat(NEURON_COUNT) { neuron ->
            dendrite[neuron] +=
                ((REST_MV - dendrite[neuron]) +
                    recurrent[neuron] + externalCurrent[neuron]) /
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

    private fun updateAssembly(input: DoubleArray) {
        val inputMean = input.average()
        val pattern = DoubleArray(ASSEMBLY_FEATURES) { index ->
            val neuron = INPUT_RANGE.first + index
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
        var eligibilitySum = 0.0
        var absoluteEligibilitySum = 0.0
        var resourceSum = 0.0
        synapses.forEach { synapse ->
            eligibilitySum += synapse.eligibility
            absoluteEligibilitySum += abs(synapse.eligibility)
            resourceSum += synapse.resource
        }
        val synapseCount = synapses.size.coerceAtLeast(1)
        result[32] = averageRate(INPUT_RANGE)
        result[33] = averageRate(EXCITATORY_RANGE)
        result[34] = averageRate(INHIBITORY_RANGE)
        result[35] = averageRate(CONTEXT_RANGE)
        result[36] = energy.average()
        result[37] = tanh(eligibilitySum / synapseCount)
        result[38] = resourceSum / synapseCount
        result[39] = if (!exposeAssemblies || activeAssembly < 0) {
            0.0
        } else {
            activeAssembly.toDouble() / max(1, MAX_ASSEMBLIES - 1)
        }
        result[40] = if (exposeAssemblies) novelty else 0.0
        result[41] =
            (TARGET_RATE_HZ / 1000.0 - averageRate(EXCITATORY_RANGE))
                .coerceIn(-1.0, 1.0)
        result[42] = tanh(dopamine)
        result[43] = energy.minOrNull() ?: 1.0
        result[44] = lastBalance
        result[45] = tanh(absoluteEligibilitySum / synapseCount)
        result[46] = if (exposeAssemblies) 1.0 - novelty else 0.0
        result[47] = 1.0
        return result
    }

    private fun averageRate(range: IntRange): Double {
        var sum = 0.0
        range.forEach { sum += fastRate[it] }
        return sum / range.count()
    }

    private fun stateBytesEstimate(): Long {
        val neuronState =
            NEURON_COUNT.toLong() * NEURON_STATE_ARRAYS * Double.SIZE_BYTES
        val delayState =
            delayed.size.toLong() * NEURON_COUNT * Double.SIZE_BYTES
        val synapseState =
            synapses.size.toLong() *
                (SYNAPSE_DOUBLE_FIELDS * Double.SIZE_BYTES +
                    SYNAPSE_INT_FIELDS * Int.SIZE_BYTES)
        val assemblyState =
            assemblies.size.toLong() * ASSEMBLY_FEATURES * Double.SIZE_BYTES
        return neuronState + delayState + synapseState + assemblyState
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

    companion object {
        const val INPUT_SIZE = 128
        const val BRIDGE_SIZE = 48
        const val NEURON_COUNT = 1_024
        const val OUT_DEGREE = 32
        const val SYNAPSE_COUNT = NEURON_COUNT * OUT_DEGREE
        const val INPUT_FANOUT = 8
        const val INPUT_PROJECTION_COUNT = INPUT_SIZE * INPUT_FANOUT
        const val DEFAULT_SEED = 0x54415441

        private const val SNAPSHOT_SCHEMA = 4
        private const val MAX_DELAY = 8
        private const val ASSEMBLY_FEATURES = INPUT_SIZE
        private const val MAX_ASSEMBLIES = 64

        private val INPUT_RANGE = 0 until INPUT_SIZE
        private val EXCITATORY_RANGE = 128..767
        private val INHIBITORY_RANGE = 768..959
        private val CONTEXT_RANGE = 960..991
        private val MOTOR_RANGE = 992..1_023
        private const val EXCITATORY_FANOUT = 5
        private const val INPUT_DIRECT_DRIVE = 28.0
        private const val INPUT_FANOUT_DRIVE = 8.0
        private const val EXCITATORY_WEIGHT_MIN = 0.9
        private const val EXCITATORY_WEIGHT_SPAN = 1.6
        private const val INHIBITORY_WEIGHT_MIN = 1.8
        private const val INHIBITORY_WEIGHT_SPAN = 1.8

        private const val REST_MV = -65.0
        private const val RESET_MV = -70.0
        private const val THRESHOLD_MV = -50.0
        private const val TAU_SOMA_MS = 20.0
        private const val TAU_DENDRITE_MS = 35.0
        private const val SOMA_DENDRITE_COUPLING = 0.22
        private const val BASE_CURRENT = 12.5
        private const val ADAPTATION_INCREMENT_MV = 1.2
        private const val ADAPTATION_DECAY = 0.99
        private const val TARGET_RATE_HZ = 6.0
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
        private const val MAX_WEIGHT = 6.0
        private const val SPIKE_ENERGY_COST = 0.025
        private const val TRANSMISSION_ENERGY_COST = 0.000075
        private const val ENERGY_RECOVERY = 0.0015
        private const val ASSEMBLY_SIMILARITY_THRESHOLD = 0.80
        private const val ASSEMBLY_DISTANCE_THRESHOLD = 0.65
        private const val ASSEMBLY_LEARNING_RATE = 0.12
        private const val ASSEMBLY_INPUT_WEIGHT = 0.55
        private const val ASSEMBLY_NEURAL_WEIGHT = 0.45
        private const val ACTIVE_USAGE_THRESHOLD = 0.10
        private const val ACTIVE_ELIGIBILITY_THRESHOLD = 0.05
        private const val ELIGIBILITY_SATURATION_FRACTION = 0.95
        private const val WEIGHT_SATURATION_FRACTION = 0.95
        private const val NEURON_STATE_ARRAYS = 8
        private const val SYNAPSE_DOUBLE_FIELDS = 6
        private const val SYNAPSE_INT_FIELDS = 4
    }
}
