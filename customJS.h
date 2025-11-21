const char* customJS = R"(
// redraw the label as a table and a canvas.
// the table has two rows. the top row are the bit values
// and the bottom row is the second within the minute.
// the canvas shows the logical high/low pin values.
// TODO rename function
// TODO rename spanElement -> element
function convertToTable(spanElement) { 
    
    const characters = Array.from(spanElement.innerHTML);

    // --- 2. Generate and temporarily insert the Table for accurate measurement ---

    // Temporarily clear the span and make it a block container for measurement
    spanElement.innerHTML = '';
    spanElement.classList.add('visualized-container'); 
    
    let dataRowContent = '<tr>';
    characters.forEach(char => {
        dataRowContent += `<td>${char}</td>`;
    });
    dataRowContent += '</tr>';

    let indexRowContent = '<tr>';
    characters.forEach((char, index) => {
        indexRowContent += `<th>${index < 10 ? "0" + index : index}</th>`;
    });
    indexRowContent += '</tr>';

    // Use a temporary ID for easy selection
    let tableHTML = '<table id="tempVisualTable" class="char-table">';
    tableHTML += '<tbody>' + dataRowContent + '</tbody>'; 
    tableHTML += '<tfoot>' + indexRowContent + '</tfoot>'; 
    tableHTML += '</table>';

    // Insert temporarily into the span (which is now the container)
    spanElement.innerHTML = tableHTML;
    const table = document.getElementById('tempVisualTable'); 
    
    // --- 3. Measure Dimensions using Layout Properties (offsetLeft/offsetWidth) ---

    const charCells = Array.from(table.querySelector('tbody tr').children);
    
    // Store cell widths, left positions, and character data for drawing
    const cellDimensions = charCells.map((cell, index) => {
        return {
            // offsetLeft provides the integer x-coordinate relative to the table (parent is the span)
            left: cell.offsetLeft, 
            // offsetWidth provides the integer width including padding and borders
            width: cell.offsetWidth, 
            char: characters[index]
        };
    });
    
    // --- 4. Create and configure the Canvas ---

    const canvas = document.createElement('canvas');
    canvas.width = table.offsetWidth; 
    canvas.height = 40;
    const ctx = canvas.getContext('2d');
    
    // --- 5. Drawing Logic using Measured Integer Coordinates ---

    const baseLine = canvas.height; // The bottom of the canvas
    const smallBoxHeight = 2; // 2px high box

    cellDimensions.forEach(dim => {
        const char = dim.char;
        const startX = dim.left;
        const cellWidth = dim.width;

        // Drawing is bottom-aligned (y-coordinate is relative to baseLine)
        if (char === 'M') {
            // M: 2px high (80% left), full height (20% right)
            
            // Use Math.round() for the split point to ensure integer width segments
            const width80 = Math.round(cellWidth * 0.8);
            const width20 = cellWidth - width80; // Guaranteed to fill the rest of the cell

            // Box 1 (2px high, 80% width, left)
            ctx.fillRect(startX, baseLine - smallBoxHeight, width80, smallBoxHeight);

            // Box 2 (Full height, 20% width, right)
            ctx.fillRect(startX + width80, baseLine - canvas.height, width20, canvas.height);

        } else if (char === '0') {
            // 0: 2px high (20% left), full height (80% right)

            const width20 = Math.round(cellWidth * 0.2);
            const width80 = cellWidth - width20; // Guaranteed to fill the rest of the cell
            
            // Box 1 (2px high, 20% width, left)
            ctx.fillRect(startX, baseLine - smallBoxHeight, width20, smallBoxHeight);

            // Box 2 (Full height, 80% width, right)
            ctx.fillRect(startX + width20, baseLine - canvas.height, width80, canvas.height);

        } else if (char === '1') {
            // 1: 2px high (50% left), full height (50% right)
            
            const width50_left = Math.round(cellWidth / 2);
            const width50_right = cellWidth - width50_left; // Guaranteed to fill the rest of the cell

            // Box 1 (2px high, 50% width, left)
            ctx.fillRect(startX, baseLine - smallBoxHeight, width50_left, smallBoxHeight);

            // Box 2 (Full height, 50% width, right)
            ctx.fillRect(startX + width50_left, baseLine - canvas.height, width50_right, canvas.height);
        }
        // For other values (including spaces), draw nothing.
    });
    
    // --- 6. Final DOM Update ---

    // Clear the span again (removing the temporary table)
    spanElement.innerHTML = '';  // TODO why? why not just add the canvas above?
    
    // Re-inject the canvas and the table in order
    spanElement.appendChild(canvas);
    
    // Remove temporary ID before final insertion
    spanElement.appendChild(table);
}

// re-draw the label every time it is updated
var labelObserver = new MutationObserver(mutationsList => {
    labelObserver.disconnect();
    const label = document.getElementById('l1');
    convertToTable(label);
    labelObserver.observe(label, {childList:true});
});

// watch the dom for the label to appear
var creationObserver = new MutationObserver(() => {
    const label = document.getElementById('l1');
    if(label) {
      creationObserver.disconnect();
      convertToTable(label);
      labelObserver.observe(label, {childList: true});
    }
});

// enable our observer after all the page content is loaded
window.addEventListener('load', function() {
  creationObserver.observe(document.body, {
      childList: true,
      subtree: true
  });
});

)";