if (typeof document !== "undefined") { // To avoid this code block in worker threads
    window.Staircase =
        typeof window.Staircase !== "undefined" ? window.Staircase : {};
    window.Staircase._onViewerReady = null;

    const moduleArg = {
        locateFile: (file, scriptDirectory) => {
            const base = scriptDirectory.endsWith("/")
                ? scriptDirectory
                : scriptDirectory + "/";
            const relativePath = file.startsWith("/")
                ? file.substring(1)
                : file;
            return base + relativePath;
        },
        onRuntimeInitialized: () => {},
        mainScriptUrlOrBlob: "./staircase.js",
        noExitRuntime: true,
    };

    createStaircaseModule(moduleArg).then(function (module) {
        window.Staircase.initializeViewers = function () {
            if (!window.Staircase.containerIds) {
                window.Staircase.containerIds = [];
            }

            if (window.Staircase.containerId) {
                window.Staircase.containerIds.push(
                    window.Staircase.containerId,
                );
            }

            if (Staircase.containerIds.length === 0) {
                console.warn(
                    "Staircase.initializeViewers function called but no" +
                        " container ids defined on Staircase object.",
                );
                return;
            }

            if (typeof window.Staircase.viewers === "undefined") {
                window.Staircase.viewers = {};
            }

            if (typeof Staircase.onViewerReady !== "function") {
                console.warn(
                    "Staircase.onViewerReady function is not defined." +
                        " You probably want to define this to capture" +
                        " a reference to a viewer to load STEP files",
                );
            }

            for (let containerId of window.Staircase.containerIds) {
                let viewer = new module.StaircaseViewer(containerId);
                window.Staircase.viewers[containerId] = viewer;
                if (typeof window.Staircase.onViewerReady === "function") {
                    window.Staircase.onViewerReady(viewer);
                }
            }
        };

        if (typeof window.Staircase.onViewerReady !== "undefined") {
            window.Staircase.initializeViewers();
        } else {
            Object.defineProperty(window.Staircase, "onViewerReady", {
                set: function (callback) {
                    this._onViewerReady = callback;
                    window.Staircase.initializeViewers();
                },
                get: function () {
                    return this._onViewerReady;
                },
            });
        }
    });
}
