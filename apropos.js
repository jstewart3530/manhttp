const ShowAllBtn = document.getElementById ("ShowAllButton");
const HideAllBtn = document.getElementById ("HideAllButton");
const BySectionBtn = document.getElementById ("BySectionButton");
const ByNameBtn = document.getElementById ("ByNameButton");
const ResultsDiv = document.getElementById ("Results");

let SortMode = null;



function CompareBySection
   (left, right)

{
  let t = left [1].localeCompare (right [1]);
  return (t == 0) ? (left [0].localeCompare (right [0])) : t;
}


function CompareByName
   (left, right)

{
  return left [0].localeCompare (right [0]);
}


function ShowSection
   (section,
    state)

{
  let content = document.getElementById ("Section_" + section);
  let fCurrentState = (content.getAttribute ("State") != "HIDDEN");
  let fShow;


  switch (state)
  {
    case "show":     fShow = true;            break;
    case "hide":     fShow = false;           break;
    case "toggle":   fShow = !fCurrentState;  break;
    default:         return;
  }


  if (fShow == fCurrentState)
    return;

  let button = document.getElementById (`Section_${section}_HideButton`);
  let header = document.getElementById (`Section_${section}_HeaderBar`);

  let v = fShow ? [ "SHOWN", `${content.scrollHeight}px` ]
                : [ "HIDDEN", "0px" ];


  content.style.maxHeight = v [1];
  content.setAttribute ("State", v [0]);
  button.setAttribute ("State", v [0]);
  header.setAttribute ("State", v [0]);
}


function ShowAll
   ()

{
  for (let s of this.SectionNames)
  {
    ShowSection (s, this.state);
  }
}


function SortByName
   (event)

{
  if (SortMode === "BYNAME")
  	return;

  SortMode = "BYNAME";


  ByNameBtn.setAttribute ("Active", "1");
  BySectionBtn.removeAttribute ("Active");
  ShowAllBtn.onclick = function (event) { };
  ShowAllBtn.setAttribute ("Disabled", "1");

  HideAllBtn.onclick = function (event) { };
  HideAllBtn.setAttribute ("Disabled", "1");

  results.sort (CompareByName);


  for (let c of [... ResultsDiv.children].reverse ())
  {
  	ResultsDiv.removeChild (c);
  }


  let table = document.createElement ("table"); 
  table.className = "AproposTable";
  ResultsDiv.append (table);

  let row, cell, content;
  
  row = document.createElement ("tr");
  row.className = "AproposTableHeader";
  table.append (row);
  
  cell = document.createElement ("th");
  cell.setAttribute ("Column", "page");
  cell.textContent = "Page";
  row.append (cell);
  
  cell = document.createElement ("th");
  cell.setAttribute ("Column", "description");
  cell.textContent = "Description";
  row.append (cell);
  
  
  let n = results.length;
  for (let i = 0; i < n; i++)
  {
    let [page, section, description] = results [i];
    let prefix = (page.indexOf (":") >= 0) ? UriPrefix : "";
  
    row = document.createElement ("tr");
    row.className = "AproposTableRow";
    table.append (row);
  
    cell = document.createElement ("td");
    row.append (cell);
    content = document.createElement ("a");
    content.textContent = `${page}(${section})`;
    content.setAttribute ("RefType", "manpage");
    content.setAttribute ("href", `${prefix}man/${page}(${section})`);
    content.setAttribute ("target", "_blank");
    cell.append (content);
  
    cell = document.createElement ("td");
    row.append (cell);  
    cell.textContent = description;
  }
}



function SortBySection
   (event)

{
  if (SortMode === "BYSECTION")
    return;

  SortMode = "BYSECTION";


  ByNameBtn.removeAttribute ("Active");
  BySectionBtn.setAttribute ("Active", "1");

  results.sort (CompareBySection);


  for (let c of [... ResultsDiv.children].reverse ())
  {
  	ResultsDiv.removeChild (c);
  }


  let bar, HideBtn, container, table, row, cell, content;
  let PrevSection = null, n = results.length, sections = [];


  for (let i = 0; i < n; i++)
  {
    let [page, section, description] = results [i];    
    let prefix = (page.indexOf (":") >= 0) ? UriPrefix : "";
  
    if (section != PrevSection)
    {
      let ElementID = "Section_" + section;
      let description = SectionDefs [section.toLowerCase ()];
      let title = description 
                     ? `Section ${section}\u2002\u2014\u2002${description}`
                     : `Section ${section}`;


      bar = document.createElement ("div");
      bar.id = `Section_${section}_HeaderBar`;
      bar.className = "AproposSectionBar";
      bar.textContent = title;
      bar.setAttribute ("State", "SHOWN");
      ResultsDiv.append (bar);

      HideBtn = document.createElement ("span");
      HideBtn.id = `Section_${section}_HideButton`;
      HideBtn.className = "HideButton";
      HideBtn.style.marginLeft = "50px";
      HideBtn.setAttribute ("State", "SHOWN");
      HideBtn.onclick = new Function ("event", `ShowSection ("${section}", "toggle");`);
      bar.append (HideBtn);

 
      container = document.createElement ("div");
      container.id = ElementID;
      container.className = "AproposContainer";
      container.style.overflow = "hidden";
      ResultsDiv.append (container);


      table = document.createElement ("table"); 
      table.className = "AproposTable";      
      container.append (table);


      row = document.createElement ("tr");
      row.className = "AproposTableHeader";
      table.append (row);

      row = document.createElement ("tr");
      row.className = "AproposTableHeader";
      table.append (row);

      cell = document.createElement ("th");
      cell.setAttribute ("Column", "page");
      cell.textContent = "Page";
      row.append (cell);
      
      cell = document.createElement ("th");
      cell.setAttribute ("Column", "description");
      cell.textContent = "Description";
      row.append (cell);

      sections.push (section);

      PrevSection = section;
    }


    row = document.createElement ("tr");
    row.className = "AproposTableRow";
    table.append (row);

    cell = document.createElement ("td");
    row.append (cell);
    content = document.createElement ("a");
    content.textContent = page;
    content.setAttribute ("href", `${prefix}man/${page}(${section})`);
    content.setAttribute ("target", "_blank");
    content.setAttribute ("RefType", "manpage");
    cell.append (content);
  
    cell = document.createElement ("td");
    row.append (cell);  
    cell.textContent = description;
  }


  ShowAllBtn.onclick = ShowAll.bind ({ SectionNames : sections, state : "show" });
  ShowAllBtn.removeAttribute ("Disabled");

  HideAllBtn.onclick = ShowAll.bind ({ SectionNames : sections, state : "hide" });
  HideAllBtn.removeAttribute ("Disabled");
}



ByNameBtn.onclick = SortByName;
BySectionBtn.onclick = SortBySection;

SortBySection (null);

