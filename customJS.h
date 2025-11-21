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
    const CANVAS_HEIGHT = 40;
    const BOX_HEIGHT = 2;
    // Maps characters to the percentage of width allocated to the "left" (short) box.
    // 'M': 80% short, 20% tall. '0': 20% short, 80% tall.
    const DRAW_RATIOS = { 'M': 0.8, '0': 0.2, '1': 0.5 };

    // --- 1. Setup Container & Generate Table ---
    containerSpan.classList.add('visualized-container');

    // Create Table HTML efficiently
    const dataCells = characters.map(char => `<td>${char}</td>`).join('');
    const indexCells = characters.map((_, i) => `<th>${String(i).padStart(2, '0')}</th>`).join('');

    const tableHTML = `
        <table class="char-table">
            <tbody><tr>${dataCells}</tr></tbody>
            <tfoot><tr>${indexCells}</tr></tfoot>
        </table>
    `;

    // Insert the table immediately. 
    containerSpan.innerHTML = tableHTML;
    const table = containerSpan.firstElementChild;

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