import { createApp } from "vue";
import "katex/dist/katex.min.css";
import "../styles.css";
import "./theme.css";
import App from "./App.vue";
import router from "./router/index.js";

function waitForCriticalFonts(timeoutMs = 2500) {
	const fontSet = document.fonts;

	if (!fontSet?.load) {
		return Promise.resolve();
	}

	const criticalFonts = [
		fontSet.load('400 16px "Rapport Inter"'),
		fontSet.load('700 16px "Rapport Inter"'),
		fontSet.load('500 24px "Rapport Handwriting"')
	];

	return Promise.race([
		Promise.allSettled(criticalFonts).then(() => fontSet.ready),
		new Promise((resolve) => {
			window.setTimeout(resolve, timeoutMs);
		})
	]);
}

function dismissStartupLoader() {
	const startupLoader = document.getElementById("startup-loader");

	if (!startupLoader || startupLoader.classList.contains("startup-loader--hidden")) {
		return;
	}

	startupLoader.classList.add("startup-loader--hidden");

	const removeLoader = () => {
		startupLoader.remove();
	};

	startupLoader.addEventListener("transitionend", removeLoader, { once: true });
	window.setTimeout(removeLoader, 320);
}

function stopBlockedInteraction(event) {
	event.preventDefault();
	event.stopPropagation();
}

function installInteractionGuards() {
	const blockedDocumentEvents = ["copy", "cut", "contextmenu", "dragstart", "drop", "selectstart"];

	blockedDocumentEvents.forEach((eventName) => {
		document.addEventListener(eventName, stopBlockedInteraction, true);
	});

	document.addEventListener("keydown", (event) => {
		const key = typeof event.key === "string" ? event.key.toLowerCase() : "";
		const hasModifier = event.ctrlKey || event.metaKey;
		const blocksClipboardShortcut = hasModifier && ["a", "c", "x", "insert"].includes(key);
		const blocksContextMenuShortcut = key === "contextmenu" || (event.shiftKey && key === "f10");

		if (blocksClipboardShortcut || blocksContextMenuShortcut) {
			stopBlockedInteraction(event);
		}
	}, true);

	document.addEventListener("selectionchange", () => {
		const selection = window.getSelection?.();
		if (selection && !selection.isCollapsed) {
			selection.removeAllRanges();
		}
	}, true);
}

async function bootstrap() {
	installInteractionGuards();
	createApp(App).use(router).mount("#app");
	await waitForCriticalFonts();
	window.requestAnimationFrame(() => {
		dismissStartupLoader();
	});
}

bootstrap();