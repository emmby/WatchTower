#pragma once

const char* const customJS = R"(
let savedWaveformScrollLeft = 0;

// redraw the label as a table and a canvas.
// the table has rows for the bit values, second indices, bit-group labels,
// bit weights, and live decoded field values.
// the canvas shows the logical high/low pin values.
/**
 * Converts a span's text into a visualization containing a Canvas diagram and a Data Table.
 * @param {HTMLElement} containerSpan - The target DOM element.
 */
function convertToTable(containerSpan) {
    const rawText = containerSpan.textContent;
    if (!rawText.length) return;
    const characters = Array.from(rawText);

    // --- Configuration ---
    const CANVAS_HEIGHT = 20;
    const BOX_HEIGHT = 2;
    // Maps characters to the percentage of width allocated to the "left" (short) box.
    // 'M': 80% short, 20% tall. '0': 20% short, 80% tall.
    const DRAW_RATIOS = { 'M': 0.8, '0': 0.2, '1': 0.5, '-': 0.0 };

    // --- Protocol Bit Group Definitions ---
    // Each group: { label, start, end, weights?: { bitIndex: weightOrSubLabel }, decode?: { bitPattern: displayText } }
    const PROTOCOL_GROUPS = {
        WWVB: [
            { label: 'Minutes', start: 1, end: 8, weights: {1:40, 2:20, 3:10, 5:8, 6:4, 7:2, 8:1} },
            { label: 'Hours', start: 12, end: 18, weights: {12:20, 13:10, 15:8, 16:4, 17:2, 18:1} },
            { label: 'Day of year', start: 22, end: 33, weights: {22:200, 23:100, 25:80, 26:40, 27:20, 28:10, 30:8, 31:4, 32:2, 33:1} },
            { label: 'Year', start: 45, end: 53, weights: {45:80, 46:40, 47:20, 48:10, 50:8, 51:4, 52:2, 53:1} },
            { label: 'LY', start: 55, end: 55, decode: { '0': 'No', '1': 'Yes' } },
            { label: 'DST', start: 57, end: 58, weights: {57:'D1', 58:'D2'}, decode: { '00': 'Std', '10': 'Begins', '11': 'DST', '01': 'Ends' } }
        ],
        JJY: [
            { label: 'Minutes', start: 1, end: 8, weights: {1:40, 2:20, 3:10, 5:8, 6:4, 7:2, 8:1} },
            { label: 'Hours', start: 12, end: 18, weights: {12:20, 13:10, 15:8, 16:4, 17:2, 18:1} },
            { label: 'Day of year', start: 22, end: 33, weights: {22:200, 23:100, 25:80, 26:40, 27:20, 28:10, 30:8, 31:4, 32:2, 33:1} },
            { label: 'Year', start: 41, end: 48, weights: {41:80, 42:40, 43:20, 44:10, 45:8, 46:4, 47:2, 48:1} },
            { label: 'Weekday', start: 50, end: 52, weights: {50:4, 51:2, 52:1} }
        ],
        DCF77: [
            { label: 'Minutes', start: 21, end: 27, weights: {21:1, 22:2, 23:4, 24:8, 25:10, 26:20, 27:40} },
            { label: 'Hours', start: 29, end: 34, weights: {29:1, 30:2, 31:4, 32:8, 33:10, 34:20} },
            { label: 'Day', start: 36, end: 41, weights: {36:1, 37:2, 38:4, 39:8, 40:10, 41:20} },
            { label: 'Weekday', start: 42, end: 44, weights: {42:1, 43:2, 44:4} },
            { label: 'Month', start: 45, end: 49, weights: {45:1, 46:2, 47:4, 48:8, 49:10} },
            { label: 'Year', start: 50, end: 57, weights: {50:1, 51:2, 52:4, 53:8, 54:10, 55:20, 56:40, 57:80} }
        ],
        MSF: [
            { label: 'Year', start: 17, end: 24, weights: {17:80, 18:40, 19:20, 20:10, 21:8, 22:4, 23:2, 24:1} },
            { label: 'Month', start: 25, end: 29, weights: {25:10, 26:8, 27:4, 28:2, 29:1} },
            { label: 'Day', start: 30, end: 35, weights: {30:20, 31:10, 32:8, 33:4, 34:2, 35:1} },
            { label: 'Weekday', start: 36, end: 38, weights: {36:4, 37:2, 38:1} },
            { label: 'Hours', start: 39, end: 44, weights: {39:20, 40:10, 41:8, 42:4, 43:2, 44:1} },
            { label: 'Minutes', start: 45, end: 51, weights: {45:40, 46:20, 47:10, 48:8, 49:4, 50:2, 51:1} }
        ]
    };

    // --- Detect active protocol from the Signal Protocol select element ---
    let activeProtocol = 'WWVB'; // default
    const selectEls = document.querySelectorAll('select');
    for (const sel of selectEls) {
        if (sel.value && PROTOCOL_GROUPS[sel.value]) {
            activeProtocol = sel.value;
            break;
        }
    }
    const BIT_GROUPS = PROTOCOL_GROUPS[activeProtocol] || PROTOCOL_GROUPS.WWVB;

    // Build lookup maps from the group definitions
    const bitToWeight = {};
    BIT_GROUPS.forEach(g => {
        if (g.weights) {
            Object.entries(g.weights).forEach(([bit, weight]) => {
                bitToWeight[parseInt(bit, 10)] = weight;
            });
        }
    });

    // --- 1. Setup Container & Generate Table ---
    containerSpan.classList.add('visualized-container');
    containerSpan.setAttribute('tabindex', '0');
    containerSpan.setAttribute('role', 'region');
    containerSpan.setAttribute('aria-label', `${activeProtocol} broadcast waveform and decoded bit groups`);
    containerSpan.onscroll = () => {
        if (containerSpan.querySelector('canvas')) {
            savedWaveformScrollLeft = containerSpan.scrollLeft;
        }
    };

    // Create Table HTML efficiently
    const dataCells = characters.map(char => `<td>${char}</td>`).join('');
    const indexCells = characters.map((_, i) => `<th scope="col">${String(i).padStart(2, '0')}</th>`).join('');

    // Row 3: Label row - one <td> per bit group to preserve column alignment
    const labelCells = characters.map((_, i) => {
        const group = BIT_GROUPS.find(g => g.start === i);
        if (group) {
            const span = group.end - group.start + 1;
            return `<td colspan="${span}" class="group-label">${group.label}</td>`;
        }
        // Skip cells that are covered by a previous colspan
        if (BIT_GROUPS.some(g => i > g.start && i <= g.end)) return '';
        return '<td class="empty-cell"></td>';
    }).join('');

    // Row 4: Weight row - individual cells to preserve alignment
    const weightCells = characters.map((_, i) => {
        const group = BIT_GROUPS.find(g => i >= g.start && i <= g.end);
        if (!group) return '<td class="empty-cell"></td>';
        const w = bitToWeight[i];
        const edgeClasses = `${i === group.start ? ' group-start' : ''}${i === group.end ? ' group-end' : ''}`;
        return `<td class="weight-cell${edgeClasses}">${w !== undefined ? w : ''}</td>`;
    }).join('');

    // Row 5: Calculated value row - one cell per group spanning the group's columns
    const valueCells = characters.map((_, i) => {
        const group = BIT_GROUPS.find(g => g.start === i);
        if (group) {
            const span = group.end - group.start + 1;
            return `<td colspan="${span}" class="group-value" data-group="${group.label}"></td>`;
        }
        if (BIT_GROUPS.some(g => i > g.start && i <= g.end)) return '';
        return '<td class="empty-cell"></td>';
    }).join('');

    const tableHTML = `
        <table class="char-table">
            <tbody><tr>${dataCells}</tr></tbody>
            <tfoot>
                <tr class="index-row">${indexCells}</tr>
                <tr class="group-label-row">${labelCells}</tr>
                <tr class="weight-row">${weightCells}</tr>
                <tr class="group-value-row">${valueCells}</tr>
            </tfoot>
        </table>
    `;

    // Insert the table immediately.
    containerSpan.innerHTML = tableHTML;
    const table = containerSpan.firstElementChild;

    // --- Compute group values from bit data ---
    BIT_GROUPS.forEach(group => {
        let displayValue = '';
        if (group.decode) {
            const bitPattern = characters.slice(group.start, group.end + 1).join('');
            displayValue = group.decode[bitPattern] || '';
        } else if (group.weights) {
            let value = 0;
            Object.entries(group.weights).forEach(([bit, weight]) => {
                const idx = parseInt(bit, 10);
                if (idx < characters.length && characters[idx] === '1') {
                    value += weight;
                }
            });
            displayValue = value;
        }
        const cell = table.querySelector(`.group-value[data-group="${group.label}"]`);
        if (cell) cell.textContent = displayValue;
    });

    // --- 2. Measure Dimensions ---
    // We map over the cells to get precise pixel measurements based on CSS rendering
    const cells = Array.from(table.querySelectorAll('tbody td'));
    const layoutMap = cells.map((cell, index) => ({
        char: characters[index],
        left: cell.offsetLeft,
        width: cell.offsetWidth
    }));

    // --- 3. Configure Canvas ---
    const canvas = document.createElement('canvas');
    canvas.setAttribute('aria-hidden', 'true');
    canvas.width = table.offsetWidth;
    canvas.height = CANVAS_HEIGHT;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#047857';

    // --- 4. Draw ---
    const baseLine = CANVAS_HEIGHT;

    layoutMap.forEach(({ char, left, width }) => {
        const ratio = DRAW_RATIOS[char];

        // If character is not in our config (e.g., spaces), skip drawing
        if (ratio === undefined) return;

        // Calculate split point (floor box vs tall box)
        const splitWidth = Math.round(width * ratio);
        const remainingWidth = width - splitWidth;

        // Box 1: The short "floor" box (Left side)
        ctx.fillRect(left, baseLine - BOX_HEIGHT, splitWidth, BOX_HEIGHT);

        // Box 2: The tall "wall" box (Right side)
        ctx.fillRect(left + splitWidth, 0, remainingWidth, CANVAS_HEIGHT);
    });

    // --- 5. Final Placement ---
    // This keeps the table in the DOM (preserving state/layout) and adds the canvas above it.
    containerSpan.prepend(canvas);
    if (savedWaveformScrollLeft > 0) {
        containerSpan.scrollLeft = savedWaveformScrollLeft;
    }
}

// re-draw the label every time it is updated
const masterObserver = new MutationObserver((mutations) => {
    // Waveform visualization
    const label = document.querySelector('.waveform-panel .label');
    if (label && !label.querySelector('canvas')) {
        convertToTable(label);
    }
});

// Start observing the entire DOM
// (we don't observe just the label element due to the zombie node problem.
// it works most of the time, but if the browser falls behind eg. while the
// computer is asleep, it may replace the element on waking up which breaks
// the observer.)
window.addEventListener('load', function() {
    masterObserver.observe(document.body, {
        childList: true,    // Detects if #l1 is added/removed
        subtree: true,      // Detects changes deep inside the DOM
        characterData: true // Detects text changes inside existing nodes
    });
});
)";

const char* const customCSS = R"(
.waveform-panel .visualized-container {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    width: 100%;
    padding: 0;
    border: none;
    background: none;
    text-align: left;
    overflow-x: auto;
}
.waveform-panel .char-table {
    border-collapse: collapse;
    width: 100%;
}
.waveform-panel .char-table tbody td {
    border: 1px solid #ccc;
    padding: 3px 0;
    font-family: monospace;
    font-size: 0.85em;
    white-space: pre;
    text-align: center;
    background-color: #f9f9f9;
    color: #864D0F;
}
.waveform-panel .char-table .index-row th {
    border: 1px solid #b2dfdb;
    padding: 2px 0;
    font-family: monospace;
    font-size: 0.70em;
    background-color: #e0f2f1;
    color: #00695c;
    font-weight: bold;
    text-align: center;
}
.waveform-panel .char-table tfoot .empty-cell {
    border: none;
    background: transparent;
    padding: 0;
}
.waveform-panel .char-table .group-label-row .group-label {
    background-color: #fff8e1;
    color: #5d4037;
    border: 1px solid #c8b99c;
    border-bottom: 1px solid #e0d6c3;
    padding: 2px 1px;
    font-family: sans-serif;
    font-size: 0.65em;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.02em;
    text-align: center;
    white-space: nowrap;
}
.waveform-panel .char-table .weight-row .weight-cell {
    background-color: #ffffff;
    color: #37474f;
    border-top: 1px solid #e0e0e0;
    border-bottom: 1px solid #e0e0e0;
    border-left: 1px dotted #e0e0e0;
    border-right: 1px dotted #e0e0e0;
    padding: 2px 0;
    font-family: monospace;
    font-size: 0.62em;
    font-weight: 400;
    text-align: center;
    white-space: nowrap;
}
.waveform-panel .char-table .weight-row .weight-cell.group-start {
    border-left: 1px solid #c8b99c;
}
.waveform-panel .char-table .weight-row .weight-cell.group-end {
    border-right: 1px solid #c8b99c;
}
.waveform-panel .char-table .group-value-row .group-value {
    background-color: #e8f5e9;
    color: #065f46;
    border: 1px solid #81c784;
    border-top: none;
    padding: 2px 1px;
    font-family: monospace;
    font-size: 0.75em;
    font-weight: bold;
    text-align: center;
    white-space: nowrap;
}
.waveform-panel canvas {
    margin-top: 5px;
    margin-bottom: 3px;
}
.waveform-panel h5 a {
    position: relative;
    top: -0.5em;
    font-size: 0.7em;
    margin-left: 3px;
    text-decoration: none;
    color: #FFFFFF;
}
.waveform-panel h5 a:hover,
.waveform-panel h5 a:focus-visible {
    text-decoration: underline;
}
)";
