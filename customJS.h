const char* customJS = R"(
// redraw the label as a table and a canvas.
// the table has two rows. the top row are the bit values
// and the bottom row is the second within the minute.
// the canvas shows the logical high/low pin values.
// TODO rename function
/**
 * Converts a span's text into a visualization containing a Canvas diagram and a Data Table.
 * @param {HTMLElement} containerSpan - The target DOM element.
 */
function convertToTable(containerSpan) {
    const rawText = containerSpan.textContent;
    const characters = Array.from(rawText);

    // --- Configuration ---
    const CANVAS_HEIGHT = 30;
    const BOX_HEIGHT = 2;
    // Maps characters to the percentage of width allocated to the "left" (short) box.
    // 'M': 80% short, 20% tall. '0': 20% short, 80% tall.
    const DRAW_RATIOS = { 'M': 0.8, '0': 0.2, '1': 0.5 };

    // --- WWVB Bit Group Definitions ---
    // Each group: { label, start, end, weights: { bitIndex: bcdWeight } }
    const BIT_GROUPS = [
        { label: 'Minutes', start: 1, end: 8, weights: {1:40, 2:20, 3:10, 5:8, 6:4, 7:2, 8:1} },
        { label: 'Hours', start: 12, end: 18, weights: {12:20, 13:10, 15:8, 16:4, 17:2, 18:1} },
        { label: 'Day of year', start: 22, end: 33, weights: {22:200, 23:100, 25:80, 26:40, 27:20, 28:10, 30:8, 31:4, 32:2, 33:1} },
        { label: 'Year', start: 45, end: 53, weights: {45:80, 46:40, 47:20, 48:10, 50:8, 51:4, 52:2, 53:1} }
    ];

    // Build lookup maps from the group definitions
    const bitToWeight = {};
    const bitToGroup = {};
    BIT_GROUPS.forEach(g => {
        for (let i = g.start; i <= g.end; i++) {
            bitToGroup[i] = g.label;
        }
        Object.entries(g.weights).forEach(([bit, weight]) => {
            bitToWeight[parseInt(bit)] = weight;
        });
    });

    // --- 1. Setup Container & Generate Table ---
    containerSpan.classList.add('visualized-container');
    containerSpan.style.display = 'flex';
    containerSpan.style.flexDirection = 'column';

    // Create Table HTML efficiently
    const dataCells = characters.map(char => `<td>${char}</td>`).join('');
    const indexCells = characters.map((_, i) => `<th>${String(i).padStart(2, '0')}</th>`).join('');

    // Row 3: Label row - one <td> per bit to preserve column alignment
    const labelCells = characters.map((_, i) => {
        const group = BIT_GROUPS.find(g => g.start === i);
        if (group) {
            const span = group.end - group.start + 1;
            return `<td colspan="${span}" style="text-align:center;font-size:0.7em;border-top:1px solid #888;color:#ccc">${group.label}</td>`;
        }
        // Skip cells that are covered by a previous colspan
        if (BIT_GROUPS.some(g => i > g.start && i <= g.end)) return '';
        return '<td></td>';
    }).join('');

    // Row 4: Weight row - individual cells to preserve alignment
    const weightCells = characters.map((_, i) => {
        const w = bitToWeight[i];
        return `<td style="font-size:0.65em;color:#aaa;text-align:center">${w !== undefined ? w : ''}</td>`;
    }).join('');

    // Row 5: Calculated value row - one cell per group spanning the group's columns
    const valueCells = characters.map((_, i) => {
        const group = BIT_GROUPS.find(g => g.start === i);
        if (group) {
            const span = group.end - group.start + 1;
            return `<td colspan="${span}" class="group-value" data-group="${group.label}" style="text-align:center;font-size:0.75em;font-weight:bold;border-top:1px solid #888;color:#7f7"></td>`;
        }
        if (BIT_GROUPS.some(g => i > g.start && i <= g.end)) return '';
        return '<td></td>';
    }).join('');

    const tableHTML = `
        <table class="char-table">
            <tbody><tr>${dataCells}</tr></tbody>
            <tfoot><tr>${indexCells}</tr></tfoot>
            <tfoot><tr>${labelCells}</tr></tfoot>
            <tfoot><tr>${weightCells}</tr></tfoot>
            <tfoot><tr>${valueCells}</tr></tfoot>
        </table>
    `;

    // Insert the table immediately. 
    containerSpan.innerHTML = tableHTML;
    const table = containerSpan.firstElementChild;

    // --- Compute group values from bit data ---
    BIT_GROUPS.forEach(group => {
        let value = 0;
        Object.entries(group.weights).forEach(([bit, weight]) => {
            const idx = parseInt(bit);
            if (idx < characters.length && characters[idx] === '1') {
                value += weight;
            }
        });
        const cell = table.querySelector(`.group-value[data-group="${group.label}"]`);
        if (cell) cell.textContent = value;
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
    canvas.width = table.offsetWidth;
    canvas.height = CANVAS_HEIGHT;
    canvas.style.marginTop = '5px'
    const ctx = canvas.getContext('2d');

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
}

// re-draw the label every time it is updated
const masterObserver = new MutationObserver((mutations) => {
    const label = document.getElementById('l1');
    if (!label) return;

    // If it contains the table we added, ignore this update (it was us!)
    if (label.querySelector('canvas')) return;    

    convertToTable(label);
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