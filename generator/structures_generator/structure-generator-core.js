(function () {
  "use strict";

  var DEFAULT_IDS = {
    title: "buildingTitle",
    subtitle: "buildingSubtitle",
    chunkDimensions: "chunkDimensions",
    pixelDimensions: "pixelDimensions",
    chunkSize: "chunkSize",
    exportName: "exportName",
    chunkCount: "chunkCount",
    notesList: "notesList",
    downloadButton: "downloadButton",
    downloadStatus: "downloadStatus",
    chunkGuideToggle: "chunkGuideToggle",
    masterCanvas: "masterCanvas",
    chunkPreviewGrid: "chunkPreviewGrid",
    chunkNameList: "chunkNameList"
  };

  function resolveElement(id) {
    var element = document.getElementById(id);
    if (!element) {
      throw new Error("Missing structure generator element: #" + id);
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

  function drawPixelsToContext(context, structure, startX, startY, width, height) {
    for (var y = 0; y < height; y += 1) {
      for (var x = 0; x < width; x += 1) {
        var tile = structure.get(startX + x, startY + y);
        context.fillStyle = structure.palette[tile];
        context.fillRect(x, y, 1, 1);
      }
    }
  }

  function drawChunkGuides(context, structure) {
    context.save();
    context.strokeStyle = "rgba(255, 232, 198, 0.28)";
    context.lineWidth = 1;

    for (var x = structure.chunkSize; x < structure.widthPixels; x += structure.chunkSize) {
      context.beginPath();
      context.moveTo(x + 0.5, 0);
      context.lineTo(x + 0.5, structure.heightPixels);
      context.stroke();
    }

    for (var y = structure.chunkSize; y < structure.heightPixels; y += structure.chunkSize) {
      context.beginPath();
      context.moveTo(0, y + 0.5);
      context.lineTo(structure.widthPixels, y + 0.5);
      context.stroke();
    }

    context.strokeStyle = "rgba(255, 232, 198, 0.56)";
    context.strokeRect(0.5, 0.5, structure.widthPixels - 1, structure.heightPixels - 1);
    context.restore();
  }

  function setCanvasDisplaySize(canvas, widthPixels, heightPixels, scale) {
    canvas.width = widthPixels;
    canvas.height = heightPixels;
    canvas.style.width = widthPixels * scale + "px";
    canvas.style.height = heightPixels * scale + "px";
    canvas.classList.add("pixel-canvas");
  }

  function PixelStructure(config) {
    if (!config.palette) {
      throw new Error("Structure generator requires a palette.");
    }

    this.palette = config.palette;
    this.defaultTile = config.defaultTile || Object.keys(config.palette)[0];
    this.buildingName = config.buildingName || "structure";
    this.chunkSize = config.chunkSize || 16;
    this.widthChunks = config.widthChunks;
    this.heightChunks = config.heightChunks;
    this.widthPixels = this.widthChunks * this.chunkSize;
    this.heightPixels = this.heightChunks * this.chunkSize;
    this.cells = buildCellMatrix(this.heightPixels, this.widthPixels, this.defaultTile);
    this.config = config;

    if (!this.palette[this.defaultTile]) {
      throw new Error("Unknown default tile '" + this.defaultTile + "'.");
    }
  }

  PixelStructure.prototype.inBounds = function (x, y) {
    return x >= 0 && x < this.widthPixels && y >= 0 && y < this.heightPixels;
  };

  PixelStructure.prototype.ensureTile = function (tile) {
    if (!this.palette[tile]) {
      throw new Error("Unknown palette key '" + tile + "'.");
    }
  };

  PixelStructure.prototype.set = function (x, y, tile) {
    if (!this.inBounds(x, y)) {
      return this;
    }
    this.ensureTile(tile);
    this.cells[y][x] = tile;
    return this;
  };

  PixelStructure.prototype.get = function (x, y) {
    if (!this.inBounds(x, y)) {
      return this.defaultTile;
    }
    return this.cells[y][x];
  };

  PixelStructure.prototype.fillRect = function (x, y, width, height, tile) {
    for (var py = y; py < y + height; py += 1) {
      for (var px = x; px < x + width; px += 1) {
        this.set(px, py, tile);
      }
    }
    return this;
  };

  PixelStructure.prototype.strokeRect = function (x, y, width, height, tile) {
    if (width <= 0 || height <= 0) {
      return this;
    }
    this.lineH(y, x, x + width - 1, tile);
    this.lineH(y + height - 1, x, x + width - 1, tile);
    this.lineV(x, y, y + height - 1, tile);
    this.lineV(x + width - 1, y, y + height - 1, tile);
    return this;
  };

  PixelStructure.prototype.lineH = function (y, startX, endX, tile) {
    var from = Math.min(startX, endX);
    var to = Math.max(startX, endX);
    for (var x = from; x <= to; x += 1) {
      this.set(x, y, tile);
    }
    return this;
  };

  PixelStructure.prototype.lineV = function (x, startY, endY, tile) {
    var from = Math.min(startY, endY);
    var to = Math.max(startY, endY);
    for (var y = from; y <= to; y += 1) {
      this.set(x, y, tile);
    }
    return this;
  };

  PixelStructure.prototype.points = function (entries, tile) {
    for (var index = 0; index < entries.length; index += 1) {
      this.set(entries[index][0], entries[index][1], tile);
    }
    return this;
  };

  PixelStructure.prototype.checkerRect = function (x, y, width, height, primaryTile, secondaryTile, step) {
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

  PixelStructure.prototype.fillEllipse = function (centerX, centerY, radiusX, radiusY, tile) {
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

  PixelStructure.prototype.stamp = function (originX, originY, pattern, legend) {
    for (var y = 0; y < pattern.length; y += 1) {
      var row = pattern[y];
      for (var x = 0; x < row.length; x += 1) {
        var token = row[x];
        if (token === " " || token === ".") {
          continue;
        }
        if (!legend[token]) {
          throw new Error("Unknown stamp token '" + token + "'.");
        }
        this.set(originX + x, originY + y, legend[token]);
      }
    }
    return this;
  };

  PixelStructure.prototype.forEachChunk = function (callback) {
    for (var chunkY = 0; chunkY < this.heightChunks; chunkY += 1) {
      for (var chunkX = 0; chunkX < this.widthChunks; chunkX += 1) {
        callback({
          chunkX: chunkX,
          chunkY: chunkY,
          pixelX: chunkX * this.chunkSize,
          pixelY: chunkY * this.chunkSize
        });
      }
    }
  };

  PixelStructure.prototype.chunkFileName = function (chunkX, chunkY) {
    if (typeof this.config.chunkFileName === "function") {
      return this.config.chunkFileName({
        buildingName: this.buildingName,
        chunkX: chunkX,
        chunkY: chunkY,
        widthChunks: this.widthChunks,
        heightChunks: this.heightChunks,
        chunkSize: this.chunkSize
      });
    }
    return this.buildingName + "_" + (chunkX + 1) + "_" + (chunkY + 1) + ".png";
  };

  PixelStructure.prototype.zipFileName = function () {
    if (typeof this.config.zipFileName === "function") {
      return this.config.zipFileName({
        buildingName: this.buildingName,
        widthChunks: this.widthChunks,
        heightChunks: this.heightChunks
      });
    }
    return this.buildingName + "_chunks.zip";
  };

  PixelStructure.prototype.createChunkCanvas = function (chunkX, chunkY) {
    var canvas = document.createElement("canvas");
    canvas.width = this.chunkSize;
    canvas.height = this.chunkSize;
    var context = canvas.getContext("2d");
    drawPixelsToContext(
      context,
      this,
      chunkX * this.chunkSize,
      chunkY * this.chunkSize,
      this.chunkSize,
      this.chunkSize
    );
    return canvas;
  };

  function renderMainCanvas(structure, canvas, scale, showChunkGuides) {
    setCanvasDisplaySize(canvas, structure.widthPixels, structure.heightPixels, scale);
    var context = canvas.getContext("2d");
    context.imageSmoothingEnabled = false;
    drawPixelsToContext(context, structure, 0, 0, structure.widthPixels, structure.heightPixels);
    if (showChunkGuides) {
      drawChunkGuides(context, structure);
    }
  }

  function renderChunkPreviewGrid(structure, container, previewScale) {
    container.innerHTML = "";
    structure.forEachChunk(function (chunk) {
      var card = document.createElement("article");
      card.className = "chunk-card";

      var chunkLabel = document.createElement("span");
      chunkLabel.textContent = "Chunk " + (chunk.chunkX + 1) + " / " + (chunk.chunkY + 1);
      card.appendChild(chunkLabel);

      var nameLabel = document.createElement("strong");
      nameLabel.textContent = structure.chunkFileName(chunk.chunkX, chunk.chunkY);
      card.appendChild(nameLabel);

      var canvas = structure.createChunkCanvas(chunk.chunkX, chunk.chunkY);
      canvas.classList.add("pixel-canvas");
      canvas.style.width = structure.chunkSize * previewScale + "px";
      canvas.style.height = structure.chunkSize * previewScale + "px";
      card.appendChild(canvas);

      container.appendChild(card);
    });
  }

  function renderChunkNameList(structure, container) {
    container.innerHTML = "";
    structure.forEachChunk(function (chunk) {
      var row = document.createElement("div");
      row.className = "chunk-name-item";

      var label = document.createElement("span");
      label.textContent = "Chunk " + (chunk.chunkX + 1) + ", " + (chunk.chunkY + 1);
      row.appendChild(label);

      var fileName = document.createElement("strong");
      fileName.textContent = structure.chunkFileName(chunk.chunkX, chunk.chunkY);
      row.appendChild(fileName);

      container.appendChild(row);
    });
  }

  function renderNotes(notes, listElement) {
    listElement.innerHTML = "";
    for (var index = 0; index < notes.length; index += 1) {
      var item = document.createElement("li");
      item.textContent = notes[index];
      listElement.appendChild(item);
    }
  }

  function renderMeta(structure, config, elements) {
    elements.title.textContent = config.title || structure.buildingName;
    elements.subtitle.textContent = config.subtitle || "";
    elements.chunkDimensions.textContent = structure.widthChunks + " x " + structure.heightChunks;
    elements.pixelDimensions.textContent = structure.widthPixels + " x " + structure.heightPixels;
    elements.chunkSize.textContent = structure.chunkSize + " px";
    elements.exportName.textContent = structure.zipFileName();
    elements.chunkCount.textContent = (structure.widthChunks * structure.heightChunks) + " files";
    document.title = config.title || structure.buildingName;
  }

  async function downloadChunks(structure, statusElement) {
    if (!window.JSZip) {
      throw new Error("JSZip is not available. Load the page with network access or bundle JSZip locally.");
    }

    statusElement.textContent = "Packaging chunk export...";
    var zip = new window.JSZip();

    structure.forEachChunk(function (chunk) {
      var canvas = structure.createChunkCanvas(chunk.chunkX, chunk.chunkY);
      var dataUrl = canvas.toDataURL("image/png");
      zip.file(structure.chunkFileName(chunk.chunkX, chunk.chunkY), dataUrl.split(",")[1], {
        base64: true
      });
    });

    var content = await zip.generateAsync({ type: "blob" });
    var link = document.createElement("a");
    link.href = URL.createObjectURL(content);
    link.download = structure.zipFileName();
    link.click();
    URL.revokeObjectURL(link.href);
    statusElement.textContent = "ZIP ready: " + structure.zipFileName();
  }

  function createStructureGenerator(config) {
    var ids = Object.assign({}, DEFAULT_IDS, config.ids || {});
    var elements = {
      title: resolveElement(ids.title),
      subtitle: resolveElement(ids.subtitle),
      chunkDimensions: resolveElement(ids.chunkDimensions),
      pixelDimensions: resolveElement(ids.pixelDimensions),
      chunkSize: resolveElement(ids.chunkSize),
      exportName: resolveElement(ids.exportName),
      chunkCount: resolveElement(ids.chunkCount),
      notesList: resolveElement(ids.notesList),
      downloadButton: resolveElement(ids.downloadButton),
      downloadStatus: resolveElement(ids.downloadStatus),
      chunkGuideToggle: resolveElement(ids.chunkGuideToggle),
      masterCanvas: resolveElement(ids.masterCanvas),
      chunkPreviewGrid: resolveElement(ids.chunkPreviewGrid),
      chunkNameList: resolveElement(ids.chunkNameList)
    };

    var structure = new PixelStructure(config);
    config.draw(structure);

    var mainScale = clamp(config.previewScale || 8, 1, 32);
    var chunkPreviewScale = clamp(config.chunkPreviewScale || 6, 1, 16);

    function renderAll() {
      renderMeta(structure, config, elements);
      renderNotes(config.notes || [], elements.notesList);
      renderMainCanvas(structure, elements.masterCanvas, mainScale, elements.chunkGuideToggle.checked);
      renderChunkPreviewGrid(structure, elements.chunkPreviewGrid, chunkPreviewScale);
      renderChunkNameList(structure, elements.chunkNameList);
      elements.downloadStatus.textContent = "Ready to export " + (structure.widthChunks * structure.heightChunks) + " chunks.";
    }

    elements.downloadButton.addEventListener("click", function () {
      downloadChunks(structure, elements.downloadStatus).catch(function (error) {
        elements.downloadStatus.textContent = error.message;
      });
    });

    elements.chunkGuideToggle.addEventListener("change", function () {
      renderMainCanvas(structure, elements.masterCanvas, mainScale, elements.chunkGuideToggle.checked);
    });

    renderAll();

    return {
      structure: structure,
      render: renderAll,
      download: function () {
        return downloadChunks(structure, elements.downloadStatus);
      }
    };
  }

  window.createStructureGenerator = createStructureGenerator;
}());