(function () {
  "use strict";

  var DEFAULT_IDS = {
    title: "iconTitle",
    subtitle: "iconSubtitle",
    iconDimensions: "iconDimensions",
    exportName: "exportName",
    backgroundType: "backgroundType",
    exportFormat: "exportFormat",
    notesList: "notesList",
    paletteList: "paletteList",
    downloadButton: "downloadButton",
    downloadStatus: "downloadStatus",
    pixelGridToggle: "pixelGridToggle",
    masterCanvas: "masterCanvas",
    previewCanvas: "previewCanvas",
    actualCanvas: "actualCanvas"
  };

  function hasOwn(object, key) {
    return Object.prototype.hasOwnProperty.call(object, key);
  }

  function resolveElement(id) {
    var element = document.getElementById(id);
    if (!element) {
      throw new Error("Missing icon generator element: #" + id);
    }
    return element;
  }

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function buildCellMatrix(height, width, fillValue) {
    return Array.from({ length: height }, function () {
      return Array(width).fill(fillValue);
    });
  }

  function setCanvasDisplaySize(canvas, widthPixels, heightPixels, scale) {
    canvas.width = widthPixels;
    canvas.height = heightPixels;
    canvas.style.width = (widthPixels * scale) + "px";
    canvas.style.height = (heightPixels * scale) + "px";
    canvas.classList.add("pixel-canvas");
    canvas.classList.add("transparent-canvas");
  }

  function drawPixelsToContext(context, icon) {
    for (var y = 0; y < icon.heightPixels; y += 1) {
      for (var x = 0; x < icon.widthPixels; x += 1) {
        var tile = icon.get(x, y);
        var color = icon.palette[tile];
        if (!color || color === "transparent") {
          continue;
        }
        context.fillStyle = color;
        context.fillRect(x, y, 1, 1);
      }
    }
  }

  function drawPixelGrid(context, icon) {
    context.save();
    context.strokeStyle = "rgba(255, 245, 220, 0.18)";
    context.lineWidth = 1;

    for (var x = 1; x < icon.widthPixels; x += 1) {
      context.beginPath();
      context.moveTo(x + 0.5, 0);
      context.lineTo(x + 0.5, icon.heightPixels);
      context.stroke();
    }

    for (var y = 1; y < icon.heightPixels; y += 1) {
      context.beginPath();
      context.moveTo(0, y + 0.5);
      context.lineTo(icon.widthPixels, y + 0.5);
      context.stroke();
    }

    context.strokeStyle = "rgba(255, 245, 220, 0.42)";
    context.strokeRect(0.5, 0.5, icon.widthPixels - 1, icon.heightPixels - 1);
    context.restore();
  }

  function PixelIcon(config) {
    if (!config.palette) {
      throw new Error("Icon generator requires a palette.");
    }

    this.palette = config.palette;
    this.iconName = config.iconName || "icon";
    this.widthPixels = config.widthPixels || 16;
    this.heightPixels = config.heightPixels || 16;
    this.defaultTile = config.defaultTile || "transparent";
    this.config = config;
    this.cells = buildCellMatrix(this.heightPixels, this.widthPixels, this.defaultTile);

    if (!hasOwn(this.palette, this.defaultTile)) {
      throw new Error("Unknown default tile '" + this.defaultTile + "'.");
    }
  }

  PixelIcon.prototype.inBounds = function (x, y) {
    return x >= 0 && x < this.widthPixels && y >= 0 && y < this.heightPixels;
  };

  PixelIcon.prototype.ensureTile = function (tile) {
    if (!hasOwn(this.palette, tile)) {
      throw new Error("Unknown palette key '" + tile + "'.");
    }
  };

  PixelIcon.prototype.set = function (x, y, tile) {
    if (!this.inBounds(x, y)) {
      return this;
    }
    this.ensureTile(tile);
    this.cells[y][x] = tile;
    return this;
  };

  PixelIcon.prototype.get = function (x, y) {
    if (!this.inBounds(x, y)) {
      return this.defaultTile;
    }
    return this.cells[y][x];
  };

  PixelIcon.prototype.fillRect = function (x, y, width, height, tile) {
    for (var py = y; py < y + height; py += 1) {
      for (var px = x; px < x + width; px += 1) {
        this.set(px, py, tile);
      }
    }
    return this;
  };

  PixelIcon.prototype.strokeRect = function (x, y, width, height, tile) {
    if (width <= 0 || height <= 0) {
      return this;
    }
    this.lineH(y, x, x + width - 1, tile);
    this.lineH(y + height - 1, x, x + width - 1, tile);
    this.lineV(x, y, y + height - 1, tile);
    this.lineV(x + width - 1, y, y + height - 1, tile);
    return this;
  };

  PixelIcon.prototype.lineH = function (y, startX, endX, tile) {
    var from = Math.min(startX, endX);
    var to = Math.max(startX, endX);
    for (var x = from; x <= to; x += 1) {
      this.set(x, y, tile);
    }
    return this;
  };

  PixelIcon.prototype.lineV = function (x, startY, endY, tile) {
    var from = Math.min(startY, endY);
    var to = Math.max(startY, endY);
    for (var y = from; y <= to; y += 1) {
      this.set(x, y, tile);
    }
    return this;
  };

  PixelIcon.prototype.points = function (entries, tile) {
    for (var index = 0; index < entries.length; index += 1) {
      this.set(entries[index][0], entries[index][1], tile);
    }
    return this;
  };

  PixelIcon.prototype.checkerRect = function (x, y, width, height, primaryTile, secondaryTile, step) {
    var patternStep = step || 1;
    for (var py = y; py < y + height; py += 1) {
      for (var px = x; px < x + width; px += 1) {
        var cellX = Math.floor((px - x) / patternStep);
        var cellY = Math.floor((py - y) / patternStep);
        this.set(px, py, (cellX + cellY) % 2 === 0 ? primaryTile : secondaryTile);
      }
    }
    return this;
  };

  PixelIcon.prototype.fillEllipse = function (centerX, centerY, radiusX, radiusY, tile) {
    if (radiusX <= 0 || radiusY <= 0) {
      return this;
    }
    for (var y = Math.floor(centerY - radiusY); y <= Math.ceil(centerY + radiusY); y += 1) {
      for (var x = Math.floor(centerX - radiusX); x <= Math.ceil(centerX + radiusX); x += 1) {
        var nx = (x - centerX) / radiusX;
        var ny = (y - centerY) / radiusY;
        if ((nx * nx) + (ny * ny) <= 1) {
          this.set(x, y, tile);
        }
      }
    }
    return this;
  };

  PixelIcon.prototype.stamp = function (originX, originY, pattern, legend) {
    for (var y = 0; y < pattern.length; y += 1) {
      var row = pattern[y];
      for (var x = 0; x < row.length; x += 1) {
        var token = row[x];
        if (token === " " || token === ".") {
          continue;
        }
        if (!hasOwn(legend, token)) {
          throw new Error("Unknown stamp token '" + token + "'.");
        }
        this.set(originX + x, originY + y, legend[token]);
      }
    }
    return this;
  };

  PixelIcon.prototype.fileName = function () {
    if (typeof this.config.fileName === "function") {
      return this.config.fileName({
        iconName: this.iconName,
        widthPixels: this.widthPixels,
        heightPixels: this.heightPixels
      });
    }
    return this.iconName + ".png";
  };

  function renderCanvas(icon, canvas, scale, showGrid) {
    setCanvasDisplaySize(canvas, icon.widthPixels, icon.heightPixels, scale);
    var context = canvas.getContext("2d");
    context.imageSmoothingEnabled = false;
    context.clearRect(0, 0, icon.widthPixels, icon.heightPixels);
    drawPixelsToContext(context, icon);
    if (showGrid) {
      drawPixelGrid(context, icon);
    }
  }

  function renderNotes(notes, listElement) {
    listElement.innerHTML = "";
    for (var index = 0; index < notes.length; index += 1) {
      var item = document.createElement("li");
      item.textContent = notes[index];
      listElement.appendChild(item);
    }
  }

  function renderPalette(config, listElement) {
    listElement.innerHTML = "";

    var paletteOrder = config.paletteOrder || Object.keys(config.palette);
    for (var index = 0; index < paletteOrder.length; index += 1) {
      var key = paletteOrder[index];
      if (!hasOwn(config.palette, key)) {
        continue;
      }

      var item = document.createElement("div");
      item.className = "palette-item";

      var swatch = document.createElement("span");
      swatch.className = "palette-swatch";
      if (config.palette[key]) {
        swatch.style.background = config.palette[key];
      } else {
        swatch.classList.add("palette-swatch-transparent");
      }
      item.appendChild(swatch);

      var label = document.createElement("div");
      label.className = "palette-label";

      var title = document.createElement("strong");
      title.textContent = config.paletteLabels && config.paletteLabels[key] ? config.paletteLabels[key] : key;
      label.appendChild(title);

      var value = document.createElement("span");
      value.textContent = config.palette[key] || "transparent";
      label.appendChild(value);

      item.appendChild(label);
      listElement.appendChild(item);
    }
  }

  function renderMeta(icon, config, elements) {
    elements.title.textContent = config.title || icon.iconName;
    elements.subtitle.textContent = config.subtitle || "";
    elements.iconDimensions.textContent = icon.widthPixels + " x " + icon.heightPixels;
    elements.exportName.textContent = icon.fileName();
    elements.backgroundType.textContent = config.defaultBackgroundLabel || "Transparent";
    elements.exportFormat.textContent = "PNG";
    document.title = config.title || icon.iconName;
  }

  function downloadIcon(icon, statusElement) {
    statusElement.textContent = "Preparing PNG export...";

    var canvas = document.createElement("canvas");
    canvas.width = icon.widthPixels;
    canvas.height = icon.heightPixels;

    var context = canvas.getContext("2d");
    context.imageSmoothingEnabled = false;
    context.clearRect(0, 0, icon.widthPixels, icon.heightPixels);
    drawPixelsToContext(context, icon);

    var triggerDownload = function (url) {
      var link = document.createElement("a");
      link.href = url;
      link.download = icon.fileName();
      link.click();
      statusElement.textContent = "PNG ready: " + icon.fileName();
    };

    if (canvas.toBlob) {
      canvas.toBlob(function (blob) {
        if (!blob) {
          statusElement.textContent = "Unable to export PNG.";
          return;
        }
        var url = URL.createObjectURL(blob);
        triggerDownload(url);
        URL.revokeObjectURL(url);
      }, "image/png");
      return;
    }

    triggerDownload(canvas.toDataURL("image/png"));
  }

  function createIconGenerator(config) {
    var ids = Object.assign({}, DEFAULT_IDS, config.ids || {});
    var elements = {
      title: resolveElement(ids.title),
      subtitle: resolveElement(ids.subtitle),
      iconDimensions: resolveElement(ids.iconDimensions),
      exportName: resolveElement(ids.exportName),
      backgroundType: resolveElement(ids.backgroundType),
      exportFormat: resolveElement(ids.exportFormat),
      notesList: resolveElement(ids.notesList),
      paletteList: resolveElement(ids.paletteList),
      downloadButton: resolveElement(ids.downloadButton),
      downloadStatus: resolveElement(ids.downloadStatus),
      pixelGridToggle: resolveElement(ids.pixelGridToggle),
      masterCanvas: resolveElement(ids.masterCanvas),
      previewCanvas: resolveElement(ids.previewCanvas),
      actualCanvas: resolveElement(ids.actualCanvas)
    };

    var icon = new PixelIcon(config);
    config.draw(icon);

    var mainScale = clamp(config.previewScale || 20, 1, 40);
    var secondaryScale = clamp(config.secondaryScale || 10, 1, 24);

    function renderAll() {
      renderMeta(icon, config, elements);
      renderNotes(config.notes || [], elements.notesList);
      renderPalette(config, elements.paletteList);
      renderCanvas(icon, elements.masterCanvas, mainScale, elements.pixelGridToggle.checked);
      renderCanvas(icon, elements.previewCanvas, secondaryScale, false);
      renderCanvas(icon, elements.actualCanvas, 1, false);
      elements.downloadStatus.textContent = "Ready to export " + icon.fileName() + ".";
    }

    elements.downloadButton.addEventListener("click", function () {
      downloadIcon(icon, elements.downloadStatus);
    });

    elements.pixelGridToggle.addEventListener("change", function () {
      renderCanvas(icon, elements.masterCanvas, mainScale, elements.pixelGridToggle.checked);
    });

    renderAll();

    return {
      icon: icon,
      render: renderAll,
      download: function () {
        downloadIcon(icon, elements.downloadStatus);
      }
    };
  }

  window.createIconGenerator = createIconGenerator;
}());