if (typeof document !== "undefined") { // To avoid this code block in worker threads

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
        window.Staircase = window.Staircase || {};
        window.Staircase._queue = window.Staircase._queue || [];
        window.Staircase._viewers = window.Staircase._viewers || {};
        window.Staircase._containerIds = window.Staircase._containerIds || new Set();

        let ensureViewerCreated = function(containerId) {
            if (!window.Staircase._viewers[containerId]) {
                let viewer = new module.StaircaseViewer(containerId);
                window.Staircase._viewers[containerId] = viewer;
                return viewer;
            }
            return window.Staircase._viewers[containerId];
        };

        let enqueueAll = function(items) {
            for (let item of items) {
                window.Staircase._queue.push(item)
            }
        }

        let flushQueue = function() {
            while (window.Staircase._queue.length > 0) {
                let item = window.Staircase._queue.shift();
                if (!item.containerId) {
                    console.warn("Skipping invalid queue item. Missing property 'containerId'");
                    continue;
                }
                window.Staircase._containerIds.add(item.containerId);
                let viewer = ensureViewerCreated(item.containerId);
                if (item.callback) {
                    item.callback(viewer);
                }
            }
        }

        if (window.Staircase.queue) {
            enqueueAll(window.Staircase.queue);
        }

        Object.defineProperty(window.Staircase, "queue", {
            set: function (newValue) {
                enqueueAll(newValue);
                flushQueue();
            },
            get: function () {
                console.warn("Queue is write-only.")
            }
        });

        flushQueue();

    });
}
