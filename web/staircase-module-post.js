if (typeof document !== "undefined") {
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
        if (!Staircase) {
            console.error("Staircase object not defined.");
            return;
        }
        if (!Staircase.containerIds) {
            Staircase.containerIds = [];
        }

        if (Staircase.containerId) {
            Staircase.containerIds.push(Staircase.containerId)
        }
        if (Staircase.containerIds.length === 0) {
            console.error("No container ids defined on Staircase object.")
            return;
        }
        if (typeof Staircase.onViewerReady !== "function") {
            console.error("Staircase.onViewerReady function not defined on Staircase object.")
            return;
        }

        if (typeof Staircase.viewers === "undefined") {
            Staircase.viewers = {};
        }

        for (let containerId of Staircase.containerIds) {
            let viewer = new module.StaircaseViewer(containerId);
            Staircase.viewers[containerId] = viewer;
            if (typeof Staircase.onViewerReady === "function") {
                Staircase.onViewerReady(viewer);
            }
        }
    });
}
