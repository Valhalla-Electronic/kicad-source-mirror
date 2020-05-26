/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 CERN
 * @author Jon Evans <jon@craftyjon.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KICAD_PROJECT_FILE_H
#define KICAD_PROJECT_FILE_H

#include <common.h>
#include <settings/json_settings.h>


/**
 * For files like sheets and boards, a pair of that object KIID and display name
 * Display name is typically blank for the project root sheet
 */
typedef std::pair<KIID, wxString> FILE_INFO_PAIR;


/**
 * PROJECT_FILE is the backing store for a PROJECT, in JSON format.
 *
 * There is either zero or one PROJECT_FILE for every PROJECT
 * (you can have a dummy PROJECT that has no file)
 */
class PROJECT_FILE : public JSON_SETTINGS
{
public:
    /**
     * Constructs the project file for a project
     * @param aFullPath is the full disk path to the project
     */
    PROJECT_FILE( const std::string& aFullPath );

    virtual ~PROJECT_FILE() {}

    virtual bool MigrateFromLegacy( wxConfigBase* aLegacyFile ) override;

    std::vector<FILE_INFO_PAIR>& GetSheets()
    {
        return m_sheets;
    }

    std::vector<FILE_INFO_PAIR>& GetBoards()
    {
        return m_boards;
    }

protected:
    wxString getFileExt() const override;

    wxString getLegacyFileExt() const override;

private:

    /// An list of schematic sheets in this project
    std::vector<FILE_INFO_PAIR> m_sheets;

    /// A list of board files in this project
    std::vector<FILE_INFO_PAIR> m_boards;

    /**
     * Below are project-level settings that have not been moved to a dedicated file
     */
public:

    /**
     * Shared params, used by more than one application
     */

    /// The list of pinned symbol libraries
    std::vector<wxString> m_PinnedSymbolLibs;

    /// The list of pinned footprint libraries
    std::vector<wxString> m_PinnedFootprintLibs;

    /**
     * CvPcb params
     */

    /// List of equivalence (equ) files used in the project
    std::vector<wxString> m_EquivalenceFiles;
};

// Specializations to allow directly reading/writing FILE_INFO_PAIRs from JSON

void to_json( nlohmann::json& aJson, const FILE_INFO_PAIR& aPair );

void from_json( const nlohmann::json& aJson, FILE_INFO_PAIR& aPair );

#endif
