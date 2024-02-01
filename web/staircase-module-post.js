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
        window.Staircase._viewers = window.Staircase._viewers || new Map();
        window.Staircase._observers = window.Staircase._observers || new Map();
        window.Staircase._containerIds = window.Staircase._containerIds || new Set();

        let ensureViewerCreated = function(containerId) {
            if (!window.Staircase._viewers.has(containerId)) {
                let viewer = new module.StaircaseViewer(containerId);
                window.Staircase._viewers.set(containerId, viewer);
                return viewer;
            }
            return window.Staircase._viewers.get(containerId);
        };

        let enqueueAll = function(items) {
            let queuedContainerIds = new Set(window.Staircase._queue.map(item => item.containerId));

            for (let item of items) {
                if (queuedContainerIds.has(item.containerId)) {
                    window.Staircase._queue = window.Staircase._queue
                        .filter(queueItem => queueItem.containerId !== item.containerId);
                    queuedContainerIds.delete(item.containerId);
                }
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
                let containerId = item.containerId;

                let divElement = document.getElementById(containerId);

                if (!divElement) {
                    console.error("Container with id '" + containerId +
                                  "' not found.");
                    continue;
                }

                if (window.Staircase._observers.has(containerId)) {
                    window.Staircase._observers.get(containerId).disconnect();
                }

                let resizeObserver = new ResizeObserver(entries => {
                    for (let entry of entries) {
                        let rect = entry.contentRect;
                        if (rect.width > 0 && rect.height > 0) {
                            resizeObserver.disconnect();
                            window.Staircase._containerIds.add(item.containerId);
                            let viewer = ensureViewerCreated(item.containerId);
                            if (item.callback) {
                                item.callback(viewer);
                            }
                        }
                    }
                });

                resizeObserver.observe(divElement);
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

        window.Staircase.cleanUp = function() {

            for (let [containerId, viewer] of Staircase._viewers) {
                if (viewer) {
                    try {
                        module.StaircaseViewer.deleteViewer(viewer);
                    }
                    catch (e) {
                        console.log(e)
                    }
                }
            }
            window.Staircase._viewers.clear();
            window.Staircase._observers.forEach(x => x.disconnect());
            window.Staircase._observers.clear();

            window.Staircase.initialized = false;
            window.Staircase = null;
        }
        window.Staircase.initialized = true;

    });
}
