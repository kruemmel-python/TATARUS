# The game contains no reflection-based model serialization. Default optimized
# Android rules are sufficient.
-keep class org.tensorflow.lite.** { *; }
-dontwarn org.tensorflow.lite.**
