package multiversion

import org.gradle.api.artifacts.ComponentMetadataContext
import org.gradle.api.artifacts.ComponentMetadataRule

/** Select host-compatible native artifacts without changing versions or Java dependencies. */
abstract class WindowsX64MinecraftNativesRule : ComponentMetadataRule {
    override fun execute(context: ComponentMetadataContext) {
        context.details.withVariant("clientWindowsNatives") {
            withDependencies {
                removeIf { dependency ->
                    dependency.group == "org.lwjgl" && dependency.artifactSelectors.any { artifact ->
                        artifact.classifier == "natives-windows-x86" ||
                            artifact.classifier == "natives-windows-arm64"
                    }
                }
            }
        }
    }
}
