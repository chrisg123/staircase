var Staircase = typeof Staircase != "undefined" ? Staircase : {};

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
        let viewer = new module.StaircaseViewer("staircase-container");
        Staircase.viewer = viewer;
        if (typeof Staircase.onViewerReady === "function") {
            Staircase.onViewerReady(viewer);
        }
    });
}
